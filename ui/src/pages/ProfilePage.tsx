/**
 * @file        ProfilePage.tsx
 * @brief       User profile management page
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState, useEffect } from 'react'
import { useNavigate } from 'react-router-dom'
import { apiClient } from '@/api/client'
import { useAuthStore } from '@/store/authStore'
import { AppLayout } from '@/components/layout/AppLayout'
import { Input } from '@/components/ui/Input'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'

interface UserProfile {
  id: string
  email: string
  fullName: string
  tenantId: string
  createdAt: string
  totpEnabled: boolean
  oauthProviders: string[]
}

export function ProfilePage() {
  const [profile, setProfile] = useState<UserProfile | null>(null)
  const [isLoading, setIsLoading] = useState(true)
  const [isSaving, setIsSaving] = useState(false)
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')

  const [fullName, setFullName] = useState('')
  const [currentPassword, setCurrentPassword] = useState('')
  const [newPassword, setNewPassword] = useState('')
  const [confirmPassword, setConfirmPassword] = useState('')

  const user = useAuthStore((state) => state.user)
  const navigate = useNavigate()

  useEffect(() => {
    const fetchProfile = async () => {
      try {
        const response = await apiClient.get('/v1/users/me')
        setProfile(response.data)
        setFullName(response.data.fullName || '')
      } catch (err: any) {
        setError(err.response?.data?.message || 'Failed to load profile')
      } finally {
        setIsLoading(false)
      }
    }

    fetchProfile()
  }, [])

  const handleUpdateProfile = async (e: React.FormEvent) => {
    e.preventDefault()
    setError('')
    setSuccess('')
    setIsSaving(true)

    try {
      await apiClient.patch('/v1/users/me', { full_name: fullName })
      setSuccess('Profile updated successfully')
    } catch (err: any) {
      setError(err.response?.data?.message || 'Failed to update profile')
    } finally {
      setIsSaving(false)
    }
  }

  const handleChangePassword = async (e: React.FormEvent) => {
    e.preventDefault()
    setError('')
    setSuccess('')

    if (newPassword !== confirmPassword) {
      setError('New passwords do not match')
      return
    }

    if (newPassword.length < 12) {
      setError('Password must be at least 12 characters long')
      return
    }

    setIsSaving(true)

    try {
      await apiClient.post('/v1/auth/password/change', {
        current_password: currentPassword,
        new_password: newPassword,
      })
      setSuccess('Password changed successfully')
      setCurrentPassword('')
      setNewPassword('')
      setConfirmPassword('')
    } catch (err: any) {
      setError(err.response?.data?.message || 'Failed to change password')
    } finally {
      setIsSaving(false)
    }
  }

  const handleDeleteAccount = async () => {
    if (
      !confirm(
        'Are you sure you want to delete your account? This action cannot be undone. All your data will be permanently deleted.'
      )
    ) {
      return
    }

    const confirmText = prompt('Type "DELETE" to confirm account deletion:')
    if (confirmText !== 'DELETE') {
      return
    }

    try {
      await apiClient.delete('/v1/users/me')
      await useAuthStore.getState().logout()
      navigate('/login')
    } catch (err: any) {
      setError(err.response?.data?.message || 'Failed to delete account')
    }
  }

  if (isLoading) {
    return (
      <AppLayout title="Profile">
        <div className="text-center py-12">
          <div className="inline-block animate-spin rounded-full h-12 w-12 border-b-2 border-ember-orange" />
          <p className="mt-4 text-gray-600">Loading profile...</p>
        </div>
      </AppLayout>
    )
  }

  return (
    <AppLayout title="Profile">
      <div className="max-w-3xl mx-auto space-y-8">
        {/* Alerts */}
        {error && (
          <Alert type="error" onClose={() => setError('')}>
            {error}
          </Alert>
        )}
        {success && (
          <Alert type="success" onClose={() => setSuccess('')}>
            {success}
          </Alert>
        )}

        {/* Profile Information */}
        <div className="bg-white rounded-lg shadow-md p-6 border border-gray-200">
          <h2 className="text-2xl font-bold text-gray-900 mb-6">Profile Information</h2>
          <form onSubmit={handleUpdateProfile} className="space-y-6">
            <Input
              label="Email Address"
              type="email"
              value={user?.email || ''}
              disabled
              helperText="Email address cannot be changed"
            />

            <Input
              label="Full Name"
              type="text"
              value={fullName}
              onChange={(e) => setFullName(e.target.value)}
              placeholder="John Doe"
              disabled={isSaving}
            />

            <div className="bg-gray-50 p-4 rounded-lg">
              <p className="text-sm font-medium text-gray-700">Account Information</p>
              <div className="mt-2 space-y-2 text-sm text-gray-600">
                <p>
                  <span className="font-medium">Tenant ID:</span> {user?.tenantId}
                </p>
                <p>
                  <span className="font-medium">2FA Status:</span>{' '}
                  {user?.totpEnabled ? (
                    <span className="text-green-600 font-semibold">Enabled</span>
                  ) : (
                    <span className="text-yellow-600 font-semibold">Disabled</span>
                  )}
                </p>
                {profile?.oauthProviders && profile.oauthProviders.length > 0 && (
                  <p>
                    <span className="font-medium">Connected Accounts:</span>{' '}
                    {profile.oauthProviders.join(', ')}
                  </p>
                )}
              </div>
            </div>

            <Button type="submit" isLoading={isSaving}>
              Update Profile
            </Button>
          </form>
        </div>

        {/* Change Password */}
        <div className="bg-white rounded-lg shadow-md p-6 border border-gray-200">
          <h2 className="text-2xl font-bold text-gray-900 mb-6">Change Password</h2>
          <form onSubmit={handleChangePassword} className="space-y-6">
            <Input
              label="Current Password"
              type="password"
              value={currentPassword}
              onChange={(e) => setCurrentPassword(e.target.value)}
              placeholder="••••••••"
              required
              disabled={isSaving}
            />

            <Input
              label="New Password"
              type="password"
              value={newPassword}
              onChange={(e) => setNewPassword(e.target.value)}
              placeholder="••••••••"
              required
              disabled={isSaving}
              helperText="At least 12 characters with uppercase, lowercase, number, and special character"
            />

            <Input
              label="Confirm New Password"
              type="password"
              value={confirmPassword}
              onChange={(e) => setConfirmPassword(e.target.value)}
              placeholder="••••••••"
              required
              disabled={isSaving}
            />

            <Button type="submit" isLoading={isSaving}>
              Change Password
            </Button>
          </form>
        </div>

        {/* Security Settings */}
        <div className="bg-white rounded-lg shadow-md p-6 border border-gray-200">
          <h2 className="text-2xl font-bold text-gray-900 mb-6">Security Settings</h2>
          <div className="space-y-4">
            <div className="flex items-center justify-between p-4 bg-gray-50 rounded-lg">
              <div>
                <p className="font-semibold text-gray-900">Two-Factor Authentication</p>
                <p className="text-sm text-gray-600">
                  Add an extra layer of security to your account
                </p>
              </div>
              <Button variant="outline" onClick={() => navigate('/settings/2fa')}>
                Manage
              </Button>
            </div>
          </div>
        </div>

        {/* Danger Zone */}
        <div className="bg-red-50 border-2 border-red-200 rounded-lg shadow-md p-6">
          <h2 className="text-2xl font-bold text-red-900 mb-6">Danger Zone</h2>
          <div className="space-y-4">
            <div className="flex items-center justify-between">
              <div>
                <p className="font-semibold text-gray-900">Delete Account</p>
                <p className="text-sm text-gray-600">
                  Permanently delete your account and all associated data
                </p>
              </div>
              <Button variant="danger" onClick={handleDeleteAccount}>
                Delete Account
              </Button>
            </div>
          </div>
        </div>
      </div>
    </AppLayout>
  )
}
