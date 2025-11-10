/** @type {import('tailwindcss').Config} */
export default {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        'forge-blue': '#0A2540',
        'ember-orange': '#FF6B35',
        'steel-gray': '#556B7D',
        'cloud-white': '#F7F9FA',
      },
    },
  },
  plugins: [],
}
