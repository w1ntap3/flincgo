package main

import (
	"fmt"
	"log"
	"net"
	"os"

	"charm.land/bubbles/v2/textinput"
	tea "charm.land/bubbletea/v2"
	"github.com/w1ntap3/flincgo/collector/internal/decoder"
	espstub "github.com/w1ntap3/flincgo/collector/internal/esp-stub"
)

type model struct {
	textInput textinput.Model
	logs      []decoder.Log
	conn      net.PacketConn
	logTest   string
	err       error
}

func handleDatagram(m model) tea.Cmd {
	return func() tea.Msg {
		buf := make([]byte, 1024)

		n, _, err := m.conn.ReadFrom(buf)
		if err != nil {
			return errMsg{err: err}
		}
		log, err := decoder.Decode(buf[:n])
		if err != nil {
			return errMsg{err: err}
		}

		return log
	}
}

// TODO: implement

type (
	errMsg struct{ err error }
)

func (e errMsg) Error() string { return e.err.Error() }

func (m model) Init() tea.Cmd {
	return handleDatagram(m)
}

// INFO: straight from the docs:
// TLDR: cmd is handled async and the types are to write a switch case which is handled by the update function
// Internally, Cmds run asynchronously in a goroutine.
// The Msg they return is collected and sent to our update function for handling
// Remember those message types we made earlier when we were making the checkServer command?
// We handle them here. This makes dealing with many asynchronous operations very easy.
func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case decoder.Log:
		m.logs = append(m.logs, msg)

		return m, handleDatagram(m)

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
	if m.err != nil {
		return tea.NewView(fmt.Sprintf("\nWe had some trouble: %v\n\n", m.err))
	}

	var s string

	s += "Welcome to flincgo CLI :))\n\n"

	for i, log := range m.logs {
		s += fmt.Sprintf("[%v] %+v: %s\n", i, log.Header, string(log.Payload))
	}

	s += "\nFlinCGo © 2026 aura ns"

	v := tea.NewView(fmt.Sprintf("%s \n", s))
	v.AltScreen = true
	return v
}

func main() {
	err := espstub.MockEdge("127.0.0.1:20081")
	if err != nil {
		log.Printf("starting mock edge: %s", err)
		return
	}

	c, err := net.ListenPacket("udp", "127.0.0.1:20081")
	if err != nil {
		log.Fatalf("could not start server connection: %s", err)
	}
	if _, err := tea.NewProgram(model{
		conn: c,
	}).Run(); err != nil {
		os.Exit(1)
	}
}
