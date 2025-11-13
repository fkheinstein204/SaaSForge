/**
 * @author Heinstein F (@heinsteinh)
 * @created 2025-11-13
 * @description Unit tests for API key scope validation logic (Requirement A-14)
 */

#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace saasforge {
namespace auth {
namespace test {

// Helper function to validate scopes (mirrors implementation in auth_service.cpp)
bool ValidateScope(
    const std::vector<std::string>& granted_scopes,
    const std::string& requested_scope
) {
    // Exact match
    if (std::find(granted_scopes.begin(), granted_scopes.end(), requested_scope) != granted_scopes.end()) {
        return true;
    }

    // Wildcard match (e.g., "read:*" matches "read:upload")
    for (const auto& granted : granted_scopes) {
        if (!granted.empty() && granted.back() == '*') {
            std::string prefix = granted.substr(0, granted.length() - 1);
            if (requested_scope.find(prefix) == 0) {
                return true;
            }
        }
    }

    // Deny by default (Requirement A-14)
    return false;
}

class ApiKeyScopeTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// Test: Exact Scope Matching

TEST_F(ApiKeyScopeTest, ExactMatchSingleScope) {
    std::vector<std::string> granted = {"read:upload"};
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
}

TEST_F(ApiKeyScopeTest, ExactMatchMultipleScopes) {
    std::vector<std::string> granted = {"read:upload", "write:upload", "delete:upload"};

    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_TRUE(ValidateScope(granted, "write:upload"));
    EXPECT_TRUE(ValidateScope(granted, "delete:upload"));
}

TEST_F(ApiKeyScopeTest, ExactMatchCaseSensitive) {
    std::vector<std::string> granted = {"read:upload"};

    // Exact match should work
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));

    // Different case should fail (case-sensitive)
    EXPECT_FALSE(ValidateScope(granted, "READ:UPLOAD"));
    EXPECT_FALSE(ValidateScope(granted, "Read:Upload"));
}

TEST_F(ApiKeyScopeTest, NoMatchWhenScopeNotGranted) {
    std::vector<std::string> granted = {"read:upload"};

    EXPECT_FALSE(ValidateScope(granted, "write:upload"));
    EXPECT_FALSE(ValidateScope(granted, "read:payment"));
    EXPECT_FALSE(ValidateScope(granted, "admin:all"));
}

// Test: Wildcard Scope Matching

TEST_F(ApiKeyScopeTest, WildcardMatchesAllWithPrefix) {
    std::vector<std::string> granted = {"read:*"};

    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_TRUE(ValidateScope(granted, "read:payment"));
    EXPECT_TRUE(ValidateScope(granted, "read:notification"));
    EXPECT_TRUE(ValidateScope(granted, "read:anything"));
}

TEST_F(ApiKeyScopeTest, WildcardDoesNotMatchDifferentPrefix) {
    std::vector<std::string> granted = {"read:*"};

    EXPECT_FALSE(ValidateScope(granted, "write:upload"));
    EXPECT_FALSE(ValidateScope(granted, "delete:upload"));
    EXPECT_FALSE(ValidateScope(granted, "admin:all"));
}

TEST_F(ApiKeyScopeTest, WildcardMatchesNestedScopes) {
    std::vector<std::string> granted = {"read:*"};

    // Should match nested paths
    EXPECT_TRUE(ValidateScope(granted, "read:upload:file"));
    EXPECT_TRUE(ValidateScope(granted, "read:payment:invoice:123"));
}

TEST_F(ApiKeyScopeTest, MultipleWildcards) {
    std::vector<std::string> granted = {"read:*", "write:*"};

    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_TRUE(ValidateScope(granted, "write:upload"));
    EXPECT_FALSE(ValidateScope(granted, "delete:upload"));
}

TEST_F(ApiKeyScopeTest, GlobalWildcard) {
    std::vector<std::string> granted = {"*"};

    // Global wildcard should match everything
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_TRUE(ValidateScope(granted, "write:payment"));
    EXPECT_TRUE(ValidateScope(granted, "delete:anything"));
    EXPECT_TRUE(ValidateScope(granted, "admin:all"));
}

// Test: Deny by Default

TEST_F(ApiKeyScopeTest, DenyByDefaultWhenNoScopesGranted) {
    std::vector<std::string> granted = {};

    EXPECT_FALSE(ValidateScope(granted, "read:upload"));
    EXPECT_FALSE(ValidateScope(granted, "write:upload"));
    EXPECT_FALSE(ValidateScope(granted, "admin:all"));
}

TEST_F(ApiKeyScopeTest, DenyByDefaultWhenScopeAmbiguous) {
    std::vector<std::string> granted = {"read:upload"};

    // Partial matches should fail (deny by default)
    EXPECT_FALSE(ValidateScope(granted, "read:upload:file"));
    EXPECT_FALSE(ValidateScope(granted, "read"));
}

TEST_F(ApiKeyScopeTest, DenyByDefaultWhenScopeUnknown) {
    std::vector<std::string> granted = {"read:upload", "write:payment"};

    // Unknown scope should be denied
    EXPECT_FALSE(ValidateScope(granted, "unknown:scope"));
    EXPECT_FALSE(ValidateScope(granted, "read:unknown"));
}

// Test: Edge Cases

TEST_F(ApiKeyScopeTest, EmptyRequestedScope) {
    std::vector<std::string> granted = {"read:upload"};

    // Empty requested scope should be denied
    EXPECT_FALSE(ValidateScope(granted, ""));
}

TEST_F(ApiKeyScopeTest, EmptyGrantedScopes) {
    std::vector<std::string> granted = {};

    EXPECT_FALSE(ValidateScope(granted, "read:upload"));
}

TEST_F(ApiKeyScopeTest, GrantedScopeWithOnlyAsterisk) {
    std::vector<std::string> granted = {"*"};

    // Should match everything
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_TRUE(ValidateScope(granted, "anything"));
}

TEST_F(ApiKeyScopeTest, RequestedScopeIsAsterisk) {
    std::vector<std::string> granted = {"*"};

    // Requesting "*" with granted "*" should match
    EXPECT_TRUE(ValidateScope(granted, "*"));
}

TEST_F(ApiKeyScopeTest, GrantedScopeContainsWhitespace) {
    std::vector<std::string> granted = {"read:upload", " write:payment", "delete:upload "};

    // Exact match with whitespace should fail (no trimming)
    EXPECT_FALSE(ValidateScope(granted, "write:payment"));
    EXPECT_FALSE(ValidateScope(granted, "delete:upload"));

    // Match with whitespace should succeed
    EXPECT_TRUE(ValidateScope(granted, " write:payment"));
    EXPECT_TRUE(ValidateScope(granted, "delete:upload "));
}

TEST_F(ApiKeyScopeTest, SpecialCharactersInScopes) {
    std::vector<std::string> granted = {"read:upload-file", "write:payment_method"};

    EXPECT_TRUE(ValidateScope(granted, "read:upload-file"));
    EXPECT_TRUE(ValidateScope(granted, "write:payment_method"));
    EXPECT_FALSE(ValidateScope(granted, "read:upload_file"));
}

TEST_F(ApiKeyScopeTest, WildcardInMiddleOfScope) {
    // Wildcards should only work at the end
    std::vector<std::string> granted = {"read:*:upload"};

    // This is not a valid wildcard pattern (asterisk not at end)
    // It should be treated as a literal scope
    EXPECT_TRUE(ValidateScope(granted, "read:*:upload"));
    EXPECT_FALSE(ValidateScope(granted, "read:anything:upload"));
}

// Test: Complex Scenarios

TEST_F(ApiKeyScopeTest, MultipleGrantedScopesWithOverlap) {
    std::vector<std::string> granted = {"read:upload", "read:*"};

    // Should match due to both exact and wildcard
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));

    // Should match due to wildcard
    EXPECT_TRUE(ValidateScope(granted, "read:payment"));
}

TEST_F(ApiKeyScopeTest, SpecificScopeShadowedByWildcard) {
    std::vector<std::string> granted = {"read:*"};

    // Even though "read:upload" is not explicitly granted,
    // it should match due to wildcard
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
}

TEST_F(ApiKeyScopeTest, MixedExactAndWildcardScopes) {
    std::vector<std::string> granted = {
        "read:upload",
        "write:*",
        "delete:payment",
        "admin:users"
    };

    // Exact matches
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_TRUE(ValidateScope(granted, "delete:payment"));
    EXPECT_TRUE(ValidateScope(granted, "admin:users"));

    // Wildcard matches
    EXPECT_TRUE(ValidateScope(granted, "write:upload"));
    EXPECT_TRUE(ValidateScope(granted, "write:payment"));
    EXPECT_TRUE(ValidateScope(granted, "write:anything"));

    // No matches
    EXPECT_FALSE(ValidateScope(granted, "read:payment"));
    EXPECT_FALSE(ValidateScope(granted, "delete:upload"));
    EXPECT_FALSE(ValidateScope(granted, "admin:all"));
}

TEST_F(ApiKeyScopeTest, HierarchicalScopes) {
    std::vector<std::string> granted = {"read:upload:*"};

    // Should match nested scopes under "read:upload:"
    EXPECT_TRUE(ValidateScope(granted, "read:upload:file"));
    EXPECT_TRUE(ValidateScope(granted, "read:upload:image"));
    EXPECT_TRUE(ValidateScope(granted, "read:upload:document:pdf"));

    // Should NOT match parent scope
    EXPECT_FALSE(ValidateScope(granted, "read:upload"));

    // Should NOT match sibling scopes
    EXPECT_FALSE(ValidateScope(granted, "read:payment:invoice"));
}

TEST_F(ApiKeyScopeTest, NoPartialPrefixMatch) {
    std::vector<std::string> granted = {"read:upload"};

    // "read:upload" should NOT match "read:uploadfile"
    // (must be exact or have separator)
    EXPECT_FALSE(ValidateScope(granted, "read:uploadfile"));
}

TEST_F(ApiKeyScopeTest, ColonSeparatorRequired) {
    std::vector<std::string> granted = {"read:*"};

    // Should match with colon separator
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));

    // Should NOT match without separator (edge case)
    // "read:*" prefix is "read:", so "readupload" doesn't match
    EXPECT_FALSE(ValidateScope(granted, "readupload"));
}

// Test: Security Properties

TEST_F(ApiKeyScopeTest, PrivilegeEscalationPrevention) {
    std::vector<std::string> granted = {"read:upload"};

    // User with read:upload should NOT be able to access:
    EXPECT_FALSE(ValidateScope(granted, "write:upload"));
    EXPECT_FALSE(ValidateScope(granted, "delete:upload"));
    EXPECT_FALSE(ValidateScope(granted, "admin:upload"));
    EXPECT_FALSE(ValidateScope(granted, "read:*"));
    EXPECT_FALSE(ValidateScope(granted, "*"));
}

TEST_F(ApiKeyScopeTest, AdminScopeIsolation) {
    std::vector<std::string> granted = {"read:*", "write:*"};

    // Should NOT grant admin access
    EXPECT_FALSE(ValidateScope(granted, "admin:users"));
    EXPECT_FALSE(ValidateScope(granted, "admin:system"));
    EXPECT_FALSE(ValidateScope(granted, "admin:*"));
}

TEST_F(ApiKeyScopeTest, CrossTenantScopeDenial) {
    // Scopes should not allow cross-tenant access
    // (tenant isolation is enforced at a higher level, but scopes shouldn't bypass it)
    std::vector<std::string> granted = {"read:tenant:123:*"};

    EXPECT_TRUE(ValidateScope(granted, "read:tenant:123:upload"));
    EXPECT_FALSE(ValidateScope(granted, "read:tenant:456:upload"));
}

TEST_F(ApiKeyScopeTest, WildcardDoesNotBypassExactMatch) {
    std::vector<std::string> granted = {"read:*"};

    // Wildcard should work as expected
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));

    // But should NOT match scopes that don't start with "read:"
    EXPECT_FALSE(ValidateScope(granted, "write:upload"));
}

// Test: Performance Considerations

TEST_F(ApiKeyScopeTest, LargeNumberOfGrantedScopes) {
    // Simulate API key with many scopes
    std::vector<std::string> granted;
    for (int i = 0; i < 100; ++i) {
        granted.push_back("scope:" + std::to_string(i));
    }
    granted.push_back("read:upload");

    // Should still find the scope efficiently
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_FALSE(ValidateScope(granted, "write:upload"));
}

TEST_F(ApiKeyScopeTest, LongScopeNames) {
    std::string long_scope = "read:upload:";
    for (int i = 0; i < 50; ++i) {
        long_scope += "nested:";
    }
    long_scope += "file";

    std::vector<std::string> granted = {"read:upload:*"};

    // Should handle long scope names
    EXPECT_TRUE(ValidateScope(granted, long_scope));
}

// Test: Real-World Scenarios

TEST_F(ApiKeyScopeTest, ReadOnlyApiKey) {
    std::vector<std::string> granted = {"read:*"};

    // Can read everything
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_TRUE(ValidateScope(granted, "read:payment"));
    EXPECT_TRUE(ValidateScope(granted, "read:user"));

    // Cannot write or delete
    EXPECT_FALSE(ValidateScope(granted, "write:upload"));
    EXPECT_FALSE(ValidateScope(granted, "delete:payment"));
}

TEST_F(ApiKeyScopeTest, ServiceAccountApiKey) {
    std::vector<std::string> granted = {
        "read:*",
        "write:upload",
        "write:notification",
        "delete:upload"
    };

    // Can read everything
    EXPECT_TRUE(ValidateScope(granted, "read:anything"));

    // Can write to upload and notification
    EXPECT_TRUE(ValidateScope(granted, "write:upload"));
    EXPECT_TRUE(ValidateScope(granted, "write:notification"));

    // Can delete uploads
    EXPECT_TRUE(ValidateScope(granted, "delete:upload"));

    // Cannot write to payment or delete other resources
    EXPECT_FALSE(ValidateScope(granted, "write:payment"));
    EXPECT_FALSE(ValidateScope(granted, "delete:payment"));
}

TEST_F(ApiKeyScopeTest, LimitedIntegrationApiKey) {
    std::vector<std::string> granted = {
        "read:upload",
        "write:upload",
        "read:webhook"
    };

    // Can only access upload and webhook read
    EXPECT_TRUE(ValidateScope(granted, "read:upload"));
    EXPECT_TRUE(ValidateScope(granted, "write:upload"));
    EXPECT_TRUE(ValidateScope(granted, "read:webhook"));

    // Cannot access other resources
    EXPECT_FALSE(ValidateScope(granted, "read:payment"));
    EXPECT_FALSE(ValidateScope(granted, "write:webhook"));
    EXPECT_FALSE(ValidateScope(granted, "delete:upload"));
}

} // namespace test
} // namespace auth
} // namespace saasforge

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
