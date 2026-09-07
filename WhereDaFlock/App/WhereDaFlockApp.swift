import SwiftUI

@main
struct WhereDaFlockApp: App {
    @StateObject private var appState = AppState()
    @StateObject private var locationService = LocationService()
    @StateObject private var privacyService = PrivacyService()
    
    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(appState)
                .environmentObject(locationService)
                .environmentObject(privacyService)
                .onAppear {
                    setupAppearance()
                }
        }
    }
    
    private func setupAppearance() {
        let appearance = UINavigationBarAppearance()
        appearance.configureWithOpaqueBackground()
        appearance.backgroundColor = UIColor(.surface)
        appearance.titleTextAttributes = [.foregroundColor: UIColor(.textPrimary)]
        UINavigationBar.appearance().standardAppearance = appearance
        UINavigationBar.appearance().compactAppearance = appearance
        UINavigationBar.appearance().scrollEdgeAppearance = appearance
    }
}
