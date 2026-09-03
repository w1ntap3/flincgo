package main

import (
	"fmt"
	"net/http"
	"os"
	"time"

	"charm.land/bubbles/v2/textinput"
	tea "charm.land/bubbletea/v2"
)

const url = "https://charm.sh/"

type model struct {
	textInput textinput.Model
	status    int
	err       error
}

// all IO should be done through functions that implement the tea.Cmd interface and return tea.Msg
// the tut says this keeps the app simple
func checkServer() tea.Msg {
	c := &http.Client{Timeout: 10 * time.Second}

	res, err := c.Get(url)
	if err != nil {
		return errMsg{err}
	}

	return statusMsg(res.StatusCode)
}

type statusMsg int

type errMsg struct{ err error }

func (e errMsg) Error() string { return e.err.Error() }

func (m model) Init() tea.Cmd {
	return checkServer
}

// INFO: straight from the docs:
// TLDR: cmd is handled async and the types are to write a switch case which is handled by the update function
// Internally, Cmds run asynchronously in a goroutine.
// The Msg they return is collected and sent to our update function for handling
// Remember those message types we made earlier when we were making the checkServer command?
// We handle them here. This makes dealing with many asynchronous operations very easy.
func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case statusMsg:
		m.status = int(msg)
		return m, tea.Quit

	case errMsg:
		m.err = msg
		return m, tea.Quit

	case tea.KeyPressMsg:
		if msg.Mod == tea.ModCtrl && msg.Code == 'c' {
			return m, tea.Quit
		}
	}

	return m, nil
}

func (m model) View() tea.View {
	ashdhasdjhasjhdjh
	if m.err != nil {
		return tea.NewView(fmt.Sprintf("\nWe had some trouble: %v\n\n", m.err))
	}

	s := fmt.Sprintf("checking %s...", url)

	if m.status > 0 {
		s += fmt.Sprintf("%d %s!", m.status, http.StatusText(m.status))
	}

	return tea.NewView("\n" + s + "\n\n")
}

func main() {
	if _, err := tea.NewProgram(model{}).Run(); err != nil {
		fmt.Printf("uh oh, there was an error!: %v\n", err)
		os.Exit(1)
	}
}
