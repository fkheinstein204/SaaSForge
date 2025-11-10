import { Link } from 'react-router-dom'

export function NotFoundPage() {
  return (
    <div className="min-h-screen flex items-center justify-center bg-cloud-white">
      <div className="text-center">
        <h1 className="text-6xl font-bold text-forge-blue">404</h1>
        <p className="text-2xl text-steel-gray mt-4">Page not found</p>
        <Link
          to="/"
          className="mt-6 inline-block bg-ember-orange text-white py-2 px-6 rounded-md hover:bg-opacity-90"
        >
          Go home
        </Link>
      </div>
    </div>
  )
}
