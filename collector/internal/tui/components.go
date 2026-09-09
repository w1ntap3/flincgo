package tui

import "charm.land/lipgloss/v2"

var (
	Green  = lipgloss.Color("#73DACA")
	Red    = lipgloss.Color("#F10E38")
	Yellow = lipgloss.Color("#E0AF68")

	Card = lipgloss.NewStyle().
		Border(lipgloss.ASCIIBorder()).
		Padding(0, 1)

	Online = lipgloss.NewStyle().
		Bold(true).
		Foreground(Green)

	Warning = lipgloss.NewStyle().
		Foreground(Yellow)

	Error = lipgloss.NewStyle().
		Bold(true).
		Foreground(Red)

	Accent = lipgloss.NewStyle().Foreground(Green)
)
