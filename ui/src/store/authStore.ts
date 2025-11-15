import { create } from 'zustand'
import { persist } from 'zustand/middleware'
import { apiClient } from '@/api/client'

interface User {
  id: string
  email: string
  tenantId: string
  totpEnabled?: boolean
}

type OAuthProvider = 'google' | 'github' | 'microsoft'

interface AuthState {
  user: User | null
  // SECURITY FIX: Tokens no longer stored in state (use httpOnly cookies instead)
  // accessToken and refreshToken removed from state to prevent XSS attacks
  login: (email: string, password: string) => Promise<void>
  register: (email: string, password: string, fullName: string) => Promise<void>
  logout: () => Promise<void>
  refreshAccessToken: () => Promise<void>
  initiateOAuth: (provider: OAuthProvider) => Promise<void>
  requestPasswordReset: (email: string) => Promise<void>
  resetPassword: (token: string, newPassword: string) => Promise<void>
}

export const useAuthStore = create<AuthState>()(
  persist(
    (set, get) => ({
      user: null,
      // SECURITY FIX: Tokens moved to httpOnly cookies (set by backend)
      // No longer storing accessToken/refreshToken in localStorage

      login: async (email: string, password: string) => {
        const response = await apiClient.post('/v1/auth/login', { email, password })
        const { user_id, tenant_id, totp_enabled } = response.data

        // Tokens automatically set as httpOnly cookies by backend
        set({
          user: { id: user_id, email, tenantId: tenant_id, totpEnabled: totp_enabled },
        })
      },

      register: async (email: string, password: string, fullName: string) => {
        const response = await apiClient.post('/v1/auth/register', {
          email,
          password,
          full_name: fullName,
        })
        const { user_id, tenant_id } = response.data

        // Tokens automatically set as httpOnly cookies by backend
        set({
          user: { id: user_id, email, tenantId: tenant_id, totpEnabled: false },
        })
      },

      logout: async () => {
        // Refresh token sent automatically via httpOnly cookie
        await apiClient.post('/v1/auth/logout')
        set({ user: null })
      },

      refreshAccessToken: async () => {
        // Refresh token sent automatically via httpOnly cookie
        await apiClient.post('/v1/auth/refresh')
        // New tokens automatically set as httpOnly cookies by backend
        // No need to update state
      },

      initiateOAuth: async (provider: OAuthProvider) => {
        // Redirect to OAuth authorization endpoint
        const response = await apiClient.get(`/v1/oauth/${provider}/authorize`)
        const { authorization_url } = response.data
        window.location.href = authorization_url
      },

      requestPasswordReset: async (email: string) => {
        await apiClient.post('/v1/auth/password/reset/request', { email })
      },

      resetPassword: async (token: string, newPassword: string) => {
        await apiClient.post('/v1/auth/password/reset/confirm', {
          token,
          new_password: newPassword,
        })
      },
    }),
    {
      name: 'auth-storage',
    }
  )
)
