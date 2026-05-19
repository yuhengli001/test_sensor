import SwiftUI

struct VitalDashboard: View {
    @ObservedObject var bleManager: RadarBLEManager
    @State private var isRunning = false
    
    private let accentColor = Color.red
    
    var body: some View {
        ZStack {
            Color(red: 0.05, green: 0.05, blue: 0.07).ignoresSafeArea()
            
            VStack(spacing: 25) {
                // Header
                HStack {
                    VStack(alignment: .leading) {
                        Text("RADAR SENSOR")
                            .font(.caption)
                            .fontWeight(.bold)
                            .foregroundColor(.gray)
                        Text("Vital Signs")
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
                    // Main Metrics (Heart Rate)
                    VStack(spacing: 8) {
                        Image(systemName: "heart.fill")
                            .font(.system(size: 40))
                            .foregroundColor(accentColor.opacity(0.6))
                            .scaleEffect(isRunning ? 1.1 : 1.0)
                            .animation(.easeInOut(duration: 0.6).repeatForever(autoreverses: true), value: isRunning)
                        
                        Text("\(String(format: "%.1f", bleManager.vitalData.heartBpm))")
                            .font(.system(size: 80, weight: .black, design: .rounded))
                            .foregroundColor(accentColor)
                        
                        Text("HEART RATE (BPM)")
                            .font(.caption2)
                            .fontWeight(.bold)
                            .foregroundColor(.gray)
                    }
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 40)
                    .background(RoundedRectangle(cornerRadius: 30).fill(Color.white.opacity(0.03)))
                    .padding(.horizontal)
                    
                    // Secondary Metrics
                    VStack(spacing: 15) {
                        VitalMetricCard(title: "BREATHING RATE", value: String(format: "%.1f", bleManager.vitalData.breathingBpm), unit: "BPM", color: .blue)
                        VitalMetricCard(title: "DISTANCE", value: String(format: "%.2f", bleManager.vitalData.distance), unit: "m", color: .orange)
                    }
                    .padding(.horizontal)
                    
                    Spacer()
                    
                    // Control Button
                    Button(action: toggleVitals) {
                        Text(isRunning ? "STOP MONITOR" : "START MONITOR")
                            .font(.headline)
                            .fontWeight(.bold)
                            .foregroundColor(.white)
                            .frame(maxWidth: .infinity)
                            .frame(height: 60)
                            .background(isRunning ? Color.red : Color.green)
                            .clipShape(Capsule())
                            .shadow(color: (isRunning ? Color.red : Color.green).opacity(0.3), radius: 10, x: 0, y: 5)
                    }
                    .padding(.horizontal, 40)
                    .padding(.bottom, 20)
                } else {
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
                                .tint(accentColor)
                            Text("Scanning...")
                                .font(.caption)
                                .foregroundColor(accentColor)
                        }
                    }
                    Spacer()
                }
            }
        }
    }
    
    private func toggleVitals() {
        if isRunning {
            bleManager.sendStopCommand()
            isRunning = false
        } else {
            bleManager.setMode(.vitalSign)
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                bleManager.sendStartCommand()
                isRunning = true
            }
        }
    }
}

struct VitalMetricCard: View {
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
