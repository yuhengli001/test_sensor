import SwiftUI
import CoreBluetooth

struct MainTabView: View {
    @StateObject var bleManager = RadarBLEManager()
    @State private var selectedTab = 0
    
    var body: some View {
        TabView(selection: $selectedTab) {
            VibrationDashboard(bleManager: bleManager)
                .tabItem {
                    Label("Vibration", systemImage: "dot.radiowaves.left.and.right")
                }
                .tag(0)
            
            VitalDashboard(bleManager: bleManager)
                .tabItem {
                    Label("Vitals", systemImage: "heart.fill")
                }
                .tag(1)
            
            PlaceholderView(title: "Fall Detection", color: .red)
                .tabItem {
                    Label("Fall", systemImage: "figure.fall")
                }
                .tag(2)
        }
        .accentColor(.blue)
        .preferredColorScheme(.dark)
        .onAppear {
            UITabBar.appearance().backgroundColor = UIColor(red: 0.05, green: 0.05, blue: 0.07, alpha: 1.0)
        }
    }
}

struct PlaceholderView: View {
    let title: String
    let color: Color
    
    var body: some View {
        ZStack {
            Color(red: 0.05, green: 0.05, blue: 0.07).ignoresSafeArea()
            VStack {
                Image(systemName: "hammer.fill")
                    .font(.system(size: 50))
                    .foregroundColor(color)
                Text(title)
                    .font(.title)
                    .fontWeight(.black)
                Text("Algorithm Integration Pending")
                    .font(.caption)
                    .foregroundColor(.gray)
            }
        }
    }
}
