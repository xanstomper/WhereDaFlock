import SwiftUI

struct ContentView: View {
    @EnvironmentObject var appState: AppState
    @EnvironmentObject var locationService: LocationService
    @EnvironmentObject var privacyService: PrivacyService
    
    var body: some View {
        Group {
            if !appState.isOnboarded {
                OnboardingView()
            } else {
                MainTabView()
            }
        }
    }
}

struct MainTabView: View {
    @EnvironmentObject var appState: AppState
    @State private var selectedTab: AppState.Tab = .map
    
    var body: some View {
        ZStack(alignment: .bottom) {
            TabView(selection: $selectedTab) {
                MainMapView()
                    .tag(AppState.Tab.map)
                
                NavigationRouteView()
                    .tag(AppState.Tab.navigate)
                
                ReportView()
                    .tag(AppState.Tab.report)
                
                ScannerView()
                    .tag(AppState.Tab.scan)
                
                SettingsView()
                    .tag(AppState.Tab.settings)
            }
            .ignoresSafeArea(.keyboard)
            
            // Custom Tab Bar
            VStack(spacing: 0) {
                Divider()
                    .opacity(0.3)
                
                HStack(spacing: 0) {
                    ForEach(AppState.Tab.allCases, id: \.self) { tab in
                        TabButton(
                            tab: tab,
                            isSelected: selectedTab == tab,
                            action: { selectedTab = tab }
                        )
                    }
                }
                .padding(.top, 8)
                .padding(.bottom, 4)
                .background(.ultraThinMaterial)
            }
        }
    }
}

struct TabButton: View {
    let tab: AppState.Tab
    let isSelected: Bool
    let action: () -> Void
    
    var body: some View {
        Button(action: action) {
            VStack(spacing: 4) {
                Image(systemName: tab.icon)
                    .font(.system(size: 22))
                Text(tab.rawValue)
                    .font(.system(size: 11, weight: .medium))
            }
            .foregroundColor(isSelected ? .accentColor : .secondary)
            .frame(maxWidth: .infinity)
            .padding(.vertical, 4)
        }
    }
}
