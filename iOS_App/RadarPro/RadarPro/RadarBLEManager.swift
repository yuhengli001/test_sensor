import Foundation
import CoreBluetooth
import Combine

class RadarBLEManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    @Published var isConnected = false
    @Published var isScanning = false
    @Published var discoveredPeripherals: [CBPeripheral] = []
    @Published var vibrationData = VibrationData.empty
    @Published var vibrationConfig = VibrationConfig()
    @Published var vitalData = VitalData.empty
    @Published var sensorStatus: UInt8 = 0
    
    private var userWantsToConnect = false
    private var centralManager: CBCentralManager!
    private var radarPeripheral: CBPeripheral?
    
    // UUIDs matched exactly with STM32 control_service.c
    let controlServiceUUID = CBUUID(string: "0000FE40-CC7A-482A-984A-7F2ED5B3E58F")
    let activeModeUUID = CBUUID(string: "0000FE41-8E22-4541-9D4C-21EDAE82ED19")
    let systemCommandUUID = CBUUID(string: "0000FE42-8E22-4541-9D4C-21EDAE82ED19")
    let sensorStatusUUID = CBUUID(string: "0000FE43-8E22-4541-9D4C-21EDAE82ED19")
    
    // Vibration Service (Data streaming + Config)
    let vibrationServiceUUID   = CBUUID(string: "0000FE70-CC7A-482A-984A-7F2ED5B3E58F")
    let vibrationConfigUUID    = CBUUID(string: "0000FE71-8E22-4541-9D4C-21EDAE82ED19")
    let vibrationDataUUID      = CBUUID(string: "0000FE72-8E22-4541-9D4C-21EDAE82ED19")
    
    // Vital Signs Service (Data streaming)
    let vitalSignServiceUUID = CBUUID(string: "0000FE50-CC7A-482A-984A-7F2ED5B3E58F")
    let vitalDataUUID = CBUUID(string: "0000FE52-8E22-4541-9D4C-21EDAE82ED19")
    
    private var activeModeChar: CBCharacteristic?
    private var systemCommandChar: CBCharacteristic?
    private var vibrationConfigChar: CBCharacteristic?
    
    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }
    
    func startScanning() {
        guard centralManager.state == .poweredOn else { return }
        isScanning = true
        userWantsToConnect = true
        discoveredPeripherals.removeAll()
        centralManager.scanForPeripherals(withServices: nil, options: nil)
    }
    
    func stopScanning() {
        isScanning = false
        userWantsToConnect = false
        centralManager.stopScan()
    }
    
    func connect(to peripheral: CBPeripheral) {
        userWantsToConnect = false
        radarPeripheral = peripheral
        radarPeripheral?.delegate = self
        stopScanning()
        centralManager.connect(peripheral, options: nil)
    }
    
    func disconnect() {
        userWantsToConnect = false
        if let peripheral = radarPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
    }
    
    func setMode(_ mode: RadarMode) {
        guard let char = activeModeChar else { return }
        let value: UInt8 = UInt8(mode.rawValue)
        radarPeripheral?.writeValue(Data([value]), for: char, type: .withoutResponse)
    }
    
    func sendStartCommand() {
        guard let char = systemCommandChar else { return }
        print(">>> Sending START Command (0x01) to MCU")
        radarPeripheral?.writeValue(Data([1]), for: char, type: .withoutResponse)
    }
    
    func sendStopCommand() {
        guard let char = systemCommandChar else { return }
        print(">>> Sending STOP Command (0x00) to MCU")
        radarPeripheral?.writeValue(Data([0]), for: char, type: .withoutResponse)
    }

    func sendVibrationConfig() {
        guard let char = vibrationConfigChar else { return }
        let data = vibrationConfig.toData()
        print(">>> Sending VibrationConfig: preset=\(vibrationConfig.preset.label), point=\(vibrationConfig.measuredPoint)")
        radarPeripheral?.writeValue(data, for: char, type: .withResponse)
    }

    // MARK: - CBCentralManagerDelegate
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn { 
            print("Bluetooth Powered On. Waiting for user...")
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi: NSNumber) {
        let name = peripheral.name ?? "Unknown"
        print("Discovered: \(name) [RSSI: \(rssi)]")
        
        if !discoveredPeripherals.contains(where: { $0.identifier == peripheral.identifier }) {
            if name.lowercased().contains("test_sensor") {
                print(">>> MATCH FOUND: \(name).")
                discoveredPeripherals.append(peripheral)
                
                if userWantsToConnect {
                    print(">>> Attempting manual-triggered connect...")
                    connect(to: peripheral)
                }
            }
        }
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        isConnected = true
        print("Connected to \(peripheral.name ?? "Radar")")
        peripheral.discoverServices([controlServiceUUID, vibrationServiceUUID, vitalSignServiceUUID])
    }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        isConnected = false
        radarPeripheral = nil
        activeModeChar = nil
        systemCommandChar = nil
        vibrationConfigChar = nil
        print("Disconnected from Radar")
    }
    
    // MARK: - CBPeripheralDelegate
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services {
            if service.uuid == controlServiceUUID {
                peripheral.discoverCharacteristics([activeModeUUID, systemCommandUUID, sensorStatusUUID], for: service)
            } else if service.uuid == vibrationServiceUUID {
                peripheral.discoverCharacteristics([vibrationConfigUUID, vibrationDataUUID], for: service)
            } else if service.uuid == vitalSignServiceUUID {
                peripheral.discoverCharacteristics([vitalDataUUID], for: service)
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let characteristics = service.characteristics else { return }
        for characteristic in characteristics {
            if characteristic.uuid == activeModeUUID {
                activeModeChar = characteristic
            } else if characteristic.uuid == systemCommandUUID {
                systemCommandChar = characteristic
            } else if characteristic.uuid == sensorStatusUUID {
                peripheral.setNotifyValue(true, for: characteristic)
            } else if characteristic.uuid == vibrationConfigUUID {
                vibrationConfigChar = characteristic
            } else if characteristic.uuid == vibrationDataUUID {
                peripheral.setNotifyValue(true, for: characteristic)
            } else if characteristic.uuid == vitalDataUUID {
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
        print("Characteristics discovered and configured.")
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value else { return }
        
        if characteristic.uuid == sensorStatusUUID {
            sensorStatus = data[0]
        } else if characteristic.uuid == vibrationDataUUID {
            if let parsedData = parseVibrationData(data) {
                DispatchQueue.main.async {
                    self.vibrationData = parsedData
                }
            }
        } else if characteristic.uuid == vitalDataUUID {
            if let parsedData = parseVitalData(data) {
                DispatchQueue.main.async {
                    self.vitalData = parsedData
                }
            }
        }
    }
    
    private func parseVibrationData(_ data: Data) -> VibrationData? {
        guard data.count >= 28 else { return nil }

        let freq     = data.subdata(in:  0..<4).withUnsafeBytes { $0.load(as: Float.self) }
        let disp     = data.subdata(in:  4..<8).withUnsafeBytes { $0.load(as: Float.self) }
        let dispRms  = data.subdata(in:  8..<12).withUnsafeBytes { $0.load(as: Float.self) }
        let vel      = data.subdata(in: 12..<16).withUnsafeBytes { $0.load(as: Float.self) }
        let velRms   = data.subdata(in: 16..<20).withUnsafeBytes { $0.load(as: Float.self) }
        let accel    = data.subdata(in: 20..<24).withUnsafeBytes { $0.load(as: Float.self) }
        let accelRms = data.subdata(in: 24..<28).withUnsafeBytes { $0.load(as: Float.self) }

        return VibrationData(frequency: freq, displacement: disp, displacementRms: dispRms,
                             velocity: vel, velocityRms: velRms,
                             acceleration: accel, accelerationRms: accelRms)
    }
    
    private func parseVitalData(_ data: Data) -> VitalData? {
        guard data.count >= 12 else { return nil }
        
        let breathing = data.subdata(in: 0..<4).withUnsafeBytes { $0.load(as: Float.self) }
        let heart = data.subdata(in: 4..<8).withUnsafeBytes { $0.load(as: Float.self) }
        let dist = data.subdata(in: 8..<12).withUnsafeBytes { $0.load(as: Float.self) }
        
        return VitalData(breathingBpm: breathing, heartBpm: heart, distance: dist)
    }
}
