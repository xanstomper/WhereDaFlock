import Foundation
import CryptoKit
import LocalAuthentication
import Combine

class PrivacyService: ObservableObject {
    @Published var privacyStatus = PrivacyStatus()
    @Published var isGhostMode: Bool = false
    @Published var encryptionEnabled: Bool = true
    
    private let keychain = KeychainService()
    
    func enableGhostMode() {
        isGhostMode = true
        privacyStatus.isGhostMode = true
        privacyStatus.locationHistoryEnabled = false
        privacyStatus.cloudProcessingEnabled = false
        privacyStatus.analyticsEnabled = false
        UserDefaults.standard.set(true, forKey: "ghost_mode")
    }
    
    func disableGhostMode() {
        isGhostMode = false
        privacyStatus.isGhostMode = false
        UserDefaults.standard.set(false, forKey: "ghost_mode")
    }
    
    func enableEncryption() {
        encryptionEnabled = true
        privacyStatus.encryptionLevel = .maximum
    }
    
    func performPrivacyAudit() {
        privacyStatus.lastPrivacyAudit = Date()
        
        // Check UserDefaults for tracking keys
        let trackingKeys = ["analytics_enabled", "tracking_id", "ad_id"]
        for key in trackingKeys {
            if UserDefaults.standard.object(forKey: key) != nil {
                UserDefaults.standard.removeObject(forKey: key)
            }
        }
        
        privacyStatus.dataStoredBytes = calculateStoredData()
    }
    
    private func calculateStoredData() -> Int {
        // Calculate approximate stored data size
        var totalBytes = 0
        
        let domain = Bundle.main.bundleIdentifier ?? ""
        if let defaults = UserDefaults.standard.persistentDomain(forName: domain) {
            for (_, value) in defaults {
                if let data = try? JSONSerialization.data(withJSONObject: value) {
                    totalBytes += data.count
                }
            }
        }
        
        return totalBytes
    }
    
    func generateAnonymousToken() -> String {
        let randomBytes = SymmetricKey(size: .bits256)
        let token = randomBytes.withUnsafeBytes { Data(Array($0)).base64EncodedString() }
        return String(token.prefix(32))
    }
    
    func clearSessionData() {
        UserDefaults.standard.synchronize()
        performPrivacyAudit()
    }
    
    func requireBiometricAuth() async -> Bool {
        let context = LAContext()
        var error: NSError?
        
        guard context.canEvaluatePolicy(.deviceOwnerAuthenticationWithBiometrics, error: &error) else {
            return false
        }
        
        return await withCheckedContinuation { continuation in
            context.evaluatePolicy(.deviceOwnerAuthenticationWithBiometrics,
                                   localizedReason: "Access your private navigation data") { success, _ in
                continuation.resume(returning: success)
            }
        }
    }
}

// MARK: - Keychain Service
class KeychainService {
    func store(key: String, data: Data) -> Bool {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: key,
            kSecValueData as String: data,
            kSecAttrAccessible as String: kSecAttrAccessibleWhenUnlockedThisDeviceOnly
        ]
        SecItemDelete(query as CFDictionary)
        return SecItemAdd(query as CFDictionary, nil) == errSecSuccess
    }
    
    func retrieve(key: String) -> Data? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: key,
            kSecReturnData as String: true
        ]
        var result: AnyObject?
        SecItemCopyMatching(query as CFDictionary, &result)
        return result as? Data
    }
    
    func delete(key: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrAccount as String: key
        ]
        SecItemDelete(query as CFDictionary)
    }
}
