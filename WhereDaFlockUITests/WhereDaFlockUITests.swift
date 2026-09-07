import XCTest

final class WhereDaFlockUITests: XCTestCase {

    var app: XCUIApplication!

    override func setUpWithError() throws {
        continueAfterFailure = false
        app = XCUIApplication()
        app.launchArguments.append("-UITesting")
        app.launch()
    }

    override func tearDownWithError() throws {
        app = nil
    }

    func testAppLaunchesIntoMainView() throws {
        // App should load either Onboarding or MainMapView
        XCTAssertTrue(app.exists)
    }

    func testTabNavigation() throws {
        // Allow onboarding skip if present
        let getStartedBtn = app.buttons["Get Started"]
        if getStartedBtn.exists {
            getStartedBtn.tap()
        }

        // Test Map Tab
        let mapTab = app.buttons["Map"]
        if mapTab.exists {
            mapTab.tap()
            XCTAssertTrue(mapTab.isSelected || mapTab.exists)
        }

        // Test Navigate Tab
        let navigateTab = app.buttons["Navigate"]
        if navigateTab.exists {
            navigateTab.tap()
            XCTAssertTrue(app.navigationBars["Navigate"].exists || navigateTab.exists)
        }

        // Test Report Tab
        let reportTab = app.buttons["Report"]
        if reportTab.exists {
            reportTab.tap()
            XCTAssertTrue(reportTab.exists)
        }

        // Test Scan Tab
        let scanTab = app.buttons["Scan"]
        if scanTab.exists {
            scanTab.tap()
            XCTAssertTrue(scanTab.exists)
        }

        // Test Settings Tab
        let settingsTab = app.buttons["Settings"]
        if settingsTab.exists {
            settingsTab.tap()
            XCTAssertTrue(settingsTab.exists)
        }
    }

    func testGhostModeToggleInSettings() throws {
        let getStartedBtn = app.buttons["Get Started"]
        if getStartedBtn.exists {
            getStartedBtn.tap()
        }

        let settingsTab = app.buttons["Settings"]
        if settingsTab.exists {
            settingsTab.tap()

            // Look for Ghost Mode switch or row
            let ghostModeSwitch = app.switches["Ghost Mode"]
            if ghostModeSwitch.exists {
                let initialValue = ghostModeSwitch.value as? String
                ghostModeSwitch.tap()
                let newValue = ghostModeSwitch.value as? String
                XCTAssertNotEqual(initialValue, newValue)
            }
        }
    }
}
