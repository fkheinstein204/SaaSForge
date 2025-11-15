import axios from 'axios'
import { useAuthStore } from '@/store/authStore'

// SECURITY FIX: Token refresh mutex to prevent race conditions
let refreshPromise: Promise<void> | null = null

export const apiClient = axios.create({
  baseURL: import.meta.env.VITE_API_URL || 'http://localhost:8000',
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json',
  },
  // SECURITY FIX: Enable credentials to send httpOnly cookies
  withCredentials: true,
})

// SECURITY FIX: Request interceptor no longer needed
// Tokens are sent automatically via httpOnly cookies by browser
// No need to manually add Authorization header

// Response interceptor to handle token refresh with mutex
apiClient.interceptors.response.use(
  (response) => response,
  async (error) => {
    const originalRequest = error.config

    // If 401 and not already retrying, try to refresh token
    if (error.response?.status === 401 && !originalRequest._retry) {
      originalRequest._retry = true

      try {
        // SECURITY FIX: Use mutex to prevent concurrent refresh requests
        // If refresh already in progress, wait for it to complete
        if (!refreshPromise) {
          refreshPromise = useAuthStore.getState().refreshAccessToken()
        }

        // Wait for refresh to complete
        await refreshPromise
        refreshPromise = null

        // Retry original request (new tokens sent via cookies automatically)
        return apiClient(originalRequest)
      } catch (refreshError) {
        // Refresh failed, reset mutex and logout user
        refreshPromise = null
        useAuthStore.getState().logout()
        window.location.href = '/login'
        return Promise.reject(refreshError)
      }
    }

    return Promise.reject(error)
  }
)
