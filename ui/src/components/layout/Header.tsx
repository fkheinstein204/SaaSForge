/**
 * @file        Header.tsx
 * @brief       Application header with mobile menu toggle
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useAuthStore } from '@/store/authStore'

interface HeaderProps {
  onMenuClick: () => void
  title: string
}

export function Header({ onMenuClick, title }: HeaderProps) {
  const user = useAuthStore((state) => state.user)

  return (
    <header className="bg-white border-b border-gray-200 sticky top-0 z-30">
      <div className="flex items-center justify-between h-16 px-4 lg:px-8">
        {/* Mobile menu button */}
        <button
          onClick={onMenuClick}
          className="lg:hidden text-gray-600 hover:text-gray-900"
        >
          <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
            <path
              strokeLinecap="round"
              strokeLinejoin="round"
              strokeWidth={2}
              d="M4 6h16M4 12h16M4 18h16"
            />
          </svg>
        </button>

        {/* Page title */}
        <h1 className="text-xl font-semibold text-gray-900">{title}</h1>

        {/* User menu (desktop only) */}
        <div className="hidden lg:flex items-center space-x-4">
          <div className="text-right">
            <p className="text-sm font-medium text-gray-900">{user?.email}</p>
            <p className="text-xs text-gray-500">
              Tenant ID: {user?.tenantId?.slice(0, 8)}...
            </p>
          </div>
          <div className="h-10 w-10 rounded-full bg-ember-orange flex items-center justify-center text-white font-semibold">
            {user?.email?.charAt(0).toUpperCase() || 'U'}
          </div>
        </div>

        {/* Mobile placeholder */}
        <div className="lg:hidden w-6" />
      </div>
    </header>
  )
}
