import SwiftUI
import Charts

struct VibrationDashboard: View {
    @ObservedObject var bleManager: RadarBLEManager
    @State private var isRunning = false
    
    let accentColor = Color.blue
    
    var body: some View {
        ZStack {
            Color(red: 0.05, green: 0.05, blue: 0.07).ignoresSafeArea()
            
            ScrollView {
            VStack(spacing: 25) {
                // Top Header with Connect Button
                HStack {
                    VStack(alignment: .leading) {
                        Text("RADAR SENSOR")
                            .font(.caption)
                            .fontWeight(.bold)
                            .foregroundColor(.gray)
                        Text("Vibration")
                            .font(.title2)
                            .fontWeight(.black)
                            .foregroundColor(.white)
                    }
                    Spacer()
                    
                    // Connection Button
                    Button(action: {
                        if bleManager.isConnected {
                            bleManager.disconnect()
                        } else {
                            bleManager.startScanning()
                        }
                    }) {
                        Text(bleManager.isConnected ? "DISCONNECT" : "CONNECT")
                            .font(.system(size: 10, weight: .bold))
                            .padding(.horizontal, 15)
                            .padding(.vertical, 8)
                            .background(Capsule().fill(bleManager.isConnected ? Color.red.opacity(0.2) : Color.blue.opacity(0.2)))
                            .foregroundColor(bleManager.isConnected ? .red : .blue)
                    }
                }
                .padding(.horizontal)
                .padding(.top, 10)
                
                if bleManager.isConnected {
                    // Main Metrics
                    VStack(spacing: 8) {
                        Image(systemName: "dot.radiowaves.left.and.right")
                            .font(.system(size: 40))
                            .foregroundColor(accentColor.opacity(0.6))
                        
                        Text("\(String(format: "%.1f", bleManager.vibrationData.frequency))")
                            .font(.system(size: 80, weight: .black, design: .rounded))
                            .foregroundColor(accentColor)
                        Text("DOMINANT FREQUENCY (Hz)")
                            .font(.caption2)
                            .fontWeight(.bold)
                            .foregroundColor(.gray)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 40)
                    .background(RoundedRectangle(cornerRadius: 30).fill(Color.white.opacity(0.03)))
                    .padding(.horizontal)
                    
                    MetricCard(title: "DISPLACEMENT", value: String(format: "%.1f", bleManager.vibrationData.displacement), unit: "µm", color: .cyan)
                        .padding(.horizontal)

                    MetricCard(title: "VELOCITY", value: String(format: "%.2f", bleManager.vibrationData.velocity), unit: "mm/s", color: .green)
                        .padding(.horizontal)

                    MetricCard(title: "ACCELERATION", value: String(format: "%.3f", bleManager.vibrationData.acceleration), unit: "m/s²", color: .orange)
                        .padding(.horizontal)

                    // Config Section
                    VStack(alignment: .leading, spacing: 14) {
                        Text("SENSOR CONFIG")
                            .font(.caption2)
                            .fontWeight(.bold)
                            .foregroundColor(.gray)

                        // Preset
                        VStack(alignment: .leading, spacing: 6) {
                            Text("Frequency Range")
                                .font(.caption)
                                .foregroundColor(.gray)
                            Picker("Preset", selection: $bleManager.vibrationConfig.preset) {
                                ForEach(VibrationPreset.allCases, id: \.self) { p in
                                    Text(p.label).tag(p)
                                }
                            }
                            .pickerStyle(.segmented)
                        }

                        // Monitoring Point
                        HStack {
                            VStack(alignment: .leading, spacing: 2) {
                                Text("Monitoring Point")
                                    .font(.caption)
                                    .foregroundColor(.gray)
                                Text("≈ \(Int(bleManager.vibrationConfig.measuredPoint) * 25 / 100) cm")
                                    .font(.caption2)
                                    .foregroundColor(accentColor)
                            }
                            Spacer()
                            Stepper("", value: $bleManager.vibrationConfig.measuredPoint, in: 40...160)
                                .labelsHidden()
                            Text("\(bleManager.vibrationConfig.measuredPoint)")
                                .font(.body)
                                .monospacedDigit()
                                .foregroundColor(.white)
                                .frame(width: 36, alignment: .trailing)
                        }

                        Button(action: applyConfig) {
                            Text("APPLY CONFIG")
                                .font(.caption)
                                .fontWeight(.bold)
                                .foregroundColor(.white)
                                .frame(maxWidth: .infinity)
                                .frame(height: 40)
                                .background(accentColor.opacity(0.25))
                                .clipShape(Capsule())
                        }
                    }
                    .padding()
                    .background(RoundedRectangle(cornerRadius: 20).fill(Color.white.opacity(0.03)))
                    .padding(.horizontal)

                    Spacer()

                    // Control Button
                    Button(action: toggleRadar) {
                        Text(isRunning ? "STOP SENSOR" : "START MONITOR")
                            .font(.headline)
                            .fontWeight(.bold)
                            .foregroundColor(.white)
                            .frame(maxWidth: .infinity)
                            .frame(height: 60)
                            .background(isRunning ? Color.red : accentColor)
                            .clipShape(Capsule())
                            .shadow(color: (isRunning ? Color.red : accentColor).opacity(0.3), radius: 10, x: 0, y: 5)
                    }
                    .padding(.horizontal, 40)
                    .padding(.bottom, 20)
                    
                } else {
                    // Empty State / Searching
                    Spacer()
                    VStack(spacing: 20) {
                        Image(systemName: "sensor.tag.radiowaves.forward")
                            .font(.system(size: 60))
                            .foregroundColor(.gray)
                        Text("No Radar Connected")
                            .font(.headline)
                            .foregroundColor(.gray)
                        
                        if bleManager.isScanning {
                            ProgressView()
                                .tint(.blue)
                            Text("Scanning...")
                                .font(.caption)
                                .foregroundColor(.blue)
                        }
                    }
                    Spacer()
                }
            }
            } // ScrollView
        }
    }

    func toggleRadar() {
        if isRunning {
            bleManager.sendStopCommand()
        } else {
            bleManager.setMode(.vibration)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                bleManager.sendStartCommand()
            }
        }
        isRunning.toggle()
    }
    
    func applyConfig() {
        bleManager.sendVibrationConfig()
        guard isRunning else { return }
        bleManager.sendStopCommand()
        isRunning = false
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.3) {
            bleManager.setMode(.vibration)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                bleManager.sendStartCommand()
                isRunning = true
            }
        }
    }

    func statusString(_ status: UInt8) -> String {
        switch status {
        case 3: return "MEASURING"
        case 2: return "PREPARED"
        case 5: return "ERROR"
        default: return "IDLE"
        }
    }
}

struct MetricCard: View {
    let title: String
    let value: String
    let unit: String
    let color: Color
    
    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title)
                .font(.caption2)
                .fontWeight(.bold)
                .foregroundColor(.gray)
            HStack(alignment: .bottom, spacing: 2) {
                Text(value)
                    .font(.title2)
                    .fontWeight(.bold)
                    .foregroundColor(.white)
                Text(unit)
                    .font(.caption)
                    .foregroundColor(color)
                    .padding(.bottom, 4)
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .padding()
        .background(RoundedRectangle(cornerRadius: 20).fill(Color.white.opacity(0.03)))
    }
}
