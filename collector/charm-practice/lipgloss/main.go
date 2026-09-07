package main

import "charm.land/lipgloss/v2"

var style = lipgloss.NewStyle().
	Foreground(lipgloss.Color("#FAFAFA")).
	Background(lipgloss.Color("#7D56F4")).
	PaddingTop(2).
	PaddingLeft(4).
	Width(22)

func main() {
	lipgloss.Println(style.Render("hello, kitty"))
}
