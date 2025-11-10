import { create } from 'zustand'
import { persist } from 'zustand/middleware'
import { apiClient } from '@/api/client'

interface User {
  id: string
  email: string
  tenantId: string
}

interface AuthState {
  user: User | null
  accessToken: string | null
  refreshToken: string | null
  login: (email: string, password: string) => Promise<void>
  logout: () => Promise<void>
  refreshAccessToken: () => Promise<void>
}

export const useAuthStore = create<AuthState>()(
  persist(
    (set, get) => ({
      user: null,
      accessToken: null,
      refreshToken: null,

      login: async (email: string, password: string) => {
        const response = await apiClient.post('/v1/auth/login', { email, password })
        const { access_token, refresh_token, user_id, tenant_id } = response.data

        set({
          user: { id: user_id, email, tenantId: tenant_id },
          accessToken: access_token,
          refreshToken: refresh_token,
        })
      },

      logout: async () => {
        const { refreshToken } = get()
        if (refreshToken) {
          await apiClient.post('/v1/auth/logout', { refresh_token: refreshToken })
        }
        set({ user: null, accessToken: null, refreshToken: null })
      },

      refreshAccessToken: async () => {
        const { refreshToken } = get()
        if (!refreshToken) throw new Error('No refresh token')

        const response = await apiClient.post('/v1/auth/refresh', {
          refresh_token: refreshToken,
        })

        set({
          accessToken: response.data.access_token,
          refreshToken: response.data.refresh_token,
        })
      },
    }),
    {
      name: 'auth-storage',
    }
  )
)
