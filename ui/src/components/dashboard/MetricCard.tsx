/**
 * @file        MetricCard.tsx
 * @brief       Dashboard metric card component
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { ReactNode } from 'react'
import clsx from 'clsx'

interface MetricCardProps {
  title: string
  value: string | number
  icon: ReactNode
  trend?: {
    value: number
    isPositive: boolean
  }
  color?: 'blue' | 'orange' | 'green' | 'purple'
}

export function MetricCard({ title, value, icon, trend, color = 'blue' }: MetricCardProps) {
  const colorStyles = {
    blue: 'bg-blue-100 text-blue-600',
    orange: 'bg-orange-100 text-ember-orange',
    green: 'bg-green-100 text-green-600',
    purple: 'bg-purple-100 text-purple-600',
  }

  return (
    <div className="bg-white rounded-lg shadow-md p-6 border border-gray-200">
      <div className="flex items-center justify-between">
        <div className="flex-1">
          <p className="text-sm font-medium text-gray-600">{title}</p>
          <p className="mt-2 text-3xl font-bold text-gray-900">{value}</p>
          {trend && (
            <div className="mt-2 flex items-center text-sm">
              {trend.isPositive ? (
                <svg
                  className="h-4 w-4 text-green-500 mr-1"
                  fill="currentColor"
                  viewBox="0 0 20 20"
                >
                  <path
                    fillRule="evenodd"
                    d="M12 7a1 1 0 011-1h4a1 1 0 011 1v4a1 1 0 11-2 0v-1.586l-4.293 4.293a1 1 0 01-1.414 0L8 11.414l-4.293 4.293a1 1 0 01-1.414-1.414l5-5a1 1 0 011.414 0L11 11.586l3.293-3.293H13a1 1 0 01-1-1z"
                    clipRule="evenodd"
                  />
                </svg>
              ) : (
                <svg
                  className="h-4 w-4 text-red-500 mr-1"
                  fill="currentColor"
                  viewBox="0 0 20 20"
                >
                  <path
                    fillRule="evenodd"
                    d="M12 13a1 1 0 011 1v4a1 1 0 11-2 0v-1.586l-4.293-4.293a1 1 0 00-1.414 0L8 8.414l-4.293-4.293a1 1 0 00-1.414 1.414l5 5a1 1 0 001.414 0L11 8.414l3.293 3.293H13a1 1 0 00-1 1z"
                    clipRule="evenodd"
                  />
                </svg>
              )}
              <span className={trend.isPositive ? 'text-green-600' : 'text-red-600'}>
                {Math.abs(trend.value)}%
              </span>
              <span className="text-gray-500 ml-1">vs last month</span>
            </div>
          )}
        </div>
        <div className={clsx('rounded-full p-3', colorStyles[color])}>{icon}</div>
      </div>
    </div>
  )
}
