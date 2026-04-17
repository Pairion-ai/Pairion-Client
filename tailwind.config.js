/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{js,ts,jsx,tsx}"],
  theme: {
    extend: {
      colors: {
        pairion: {
          amber: "#F59E0B",
          "amber-light": "#FBBF24",
          "amber-dim": "#92400E",
          cyan: "#06B6D4",
          "cyan-light": "#22D3EE",
          "cyan-dim": "#164E63",
          purple: "#A855F7",
          teal: "#14B8A6",
          dark: "#0A0A0F",
          "dark-surface": "#111118",
          "dark-elevated": "#1A1A24",
          "dark-border": "#2A2A3A",
        },
      },
      fontFamily: {
        mono: ["JetBrains Mono", "Fira Code", "monospace"],
        sans: ["Inter", "system-ui", "sans-serif"],
      },
    },
  },
  plugins: [],
};
