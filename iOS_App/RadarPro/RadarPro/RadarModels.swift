import Foundation

struct VibrationData {
    let frequency: Float
    let displacement: Float
    let displacementRms: Float
    let velocity: Float
    let velocityRms: Float
    let acceleration: Float
    let accelerationRms: Float

    static var empty: VibrationData {
        VibrationData(frequency: 0, displacement: 0, displacementRms: 0,
                      velocity: 0, velocityRms: 0, acceleration: 0, accelerationRms: 0)
    }
}

struct VitalData {
    let breathingBpm: Float
    let heartBpm: Float
    let distance: Float
    
    static var empty: VitalData {
        VitalData(breathingBpm: 0, heartBpm: 0, distance: 0)
    }
}

enum VibrationPreset: UInt8, CaseIterable {
    case highFrequency = 0
    case lowFrequency  = 1

    var label: String {
        switch self {
        case .highFrequency: return "High (up to 5000 Hz)"
        case .lowFrequency:  return "Low (up to 100 Hz)"
        }
    }
}

struct VibrationConfig {
    var preset: VibrationPreset = .highFrequency
    var measuredPoint: Int = 80  // distance_mm = measuredPoint * 2.5

    func toData() -> Data {
        var data = Data()
        data.append(preset.rawValue)
        withUnsafeBytes(of: UInt32(measuredPoint).littleEndian) { data.append(contentsOf: $0) }
        return data
    }
}

enum RadarMode: UInt8 {
    case standby = 0
    case vitalSign = 1
    case fallDetection = 2
    case vibration = 3
}

enum SystemCommand: UInt8 {
    case stop = 0
    case start = 1
}
