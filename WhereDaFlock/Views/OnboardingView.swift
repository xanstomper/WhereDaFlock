import SwiftUI

struct OnboardingView: View {
    @EnvironmentObject var appState: AppState
    @EnvironmentObject var locationService: LocationService
    @State private var currentPage = 0
    
    let pages: [OnboardingPage] = [
        OnboardingPage(
            icon: "map.fill",
            title: "Welcome to WhereDaFlock",
            description: "Navigate freely. Stay unseen.\n\nPrivacy-first navigation with real-time road intelligence.",
            color: .blue
        ),
        OnboardingPage(
            icon: "camera.fill",
            title: "Camera Awareness",
            description: "Know where surveillance cameras are located. Our community database keeps you informed.",
            color: .orange
        ),
        OnboardingPage(
            icon: "exclamationmark.bubble.fill",
            title: "Community Intelligence",
            description: "Real-time alerts from the community. See police activity, accidents, and road hazards.",
            color: .red
        ),
        OnboardingPage(
            icon: "hand.raised.fill",
            title: "Privacy First",
            description: "Your location stays yours. No tracking. No history. No cloud storage.\n\nGhost Mode keeps you invisible.",
            color: .green
        )
    ]
    
    var body: some View {
        VStack {
            // Skip button
            HStack {
                Spacer()
                Button("Skip") {
                    completeOnboarding()
                }
                .font(.subheadline)
                .foregroundColor(.secondary)
                .padding()
            }
            
            // Pages
            TabView(selection: $currentPage) {
                ForEach(pages.indices, id: \.self) { index in
                    OnboardingPageView(page: pages[index])
                        .tag(index)
                }
            }
            .tabViewStyle(.page)
            .indexViewStyle(.page(backgroundDisplayMode: .always))
            
            // Bottom buttons
            VStack(spacing: 16) {
                if currentPage == pages.count - 1 {
                    Button(action: completeOnboarding) {
                        Text("Get Started")
                            .fontWeight(.bold)
                            .frame(maxWidth: .infinity)
                            .padding()
                            .background(Color.blue.gradient)
                            .foregroundColor(.white)
                            .cornerRadius(16)
                    }
                } else {
                    Button(action: { currentPage += 1 }) {
                        Text("Next")
                            .fontWeight(.semibold)
                            .frame(maxWidth: .infinity)
                            .padding()
                            .background(Color.blue)
                            .foregroundColor(.white)
                            .cornerRadius(16)
                    }
                }
            }
            .padding()
        }
    }
    
    private func completeOnboarding() {
        locationService.requestAuthorization()
        appState.isOnboarded = true
        appState.isGuestMode = true
    }
}

struct OnboardingPage {
    let icon: String
    let title: String
    let description: String
    let color: Color
}

struct OnboardingPageView: View {
    let page: OnboardingPage
    
    var body: some View {
        VStack(spacing: 30) {
            Image(systemName: page.icon)
                .font(.system(size: 80))
                .foregroundColor(page.color)
                .padding()
                .background(page.color.opacity(0.1))
                .clipShape(Circle())
            
            Text(page.title)
                .font(.title.bold())
                .multilineTextAlignment(.center)
            
            Text(page.description)
                .font(.body)
                .multilineTextAlignment(.center)
                .foregroundColor(.secondary)
                .padding(.horizontal, 40)
        }
        .padding()
    }
}
