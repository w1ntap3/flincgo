package tui

import (
	"fmt"
	"log"
	"net"

	tea "charm.land/bubbletea/v2"
	lg "charm.land/lipgloss/v2"
	"github.com/w1ntap3/flincgo/collector/internal/decoder"
)

type model struct {
	terminalWidth  int
	terminalHeight int
	logs           []decoder.Log
	// the index of the string we're currently at in the rendering
	conn net.PacketConn
	err  error
}

func (m model) View() tea.View {
	if m.err != nil {
		return tea.NewView(fmt.Sprintf("\nWe had some trouble: %v\n\n", m.err))
	}

	s := renderLogMatrix(m.logs, m.terminalWidth)

	v := tea.NewView(s)
	v.AltScreen = true
	return v
}

func renderLogMatrix(logs []decoder.Log, terminalWidth int) string {
	// var row int
	var s []string

	var logRow string
	for _, newLog := range logs {
		logCard := renderLogCard(newLog)
		rowWithNewLog := lg.JoinHorizontal(lg.Left, logRow, logCard)

		if lg.Width(rowWithNewLog) <= terminalWidth {
			logRow = rowWithNewLog
		} else {
			// start a new row with the current card
			s = append(s, logRow)
			logRow = logCard
		}
	}
	// if theres any leftover unfinished row, add it
	s = append(s, logRow)

	var s2 string
	for _, str := range s {
		s2 = lg.JoinVertical(lg.Left, str, s2)
	}

	return s2
}

func renderLogCard(log decoder.Log) string {
	header := fmt.Sprintf("#%d item:%d len%d", log.Header.Sequence, log.Header.ItemID, log.Header.PayloadLen)

	body := string(log.Payload)

	content := lg.JoinVertical(lg.Left, header, body)

	return Card.Margin(0, 1).Render(content)
}

type (
	errMsg struct{ err error }
)

func (e errMsg) Error() string { return e.err.Error() }

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

func (m model) Init() tea.Cmd {
	return handleDatagram(m)
}

// this is where the outputs (msg) of cmd's are handled.
func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case decoder.Log:
		m.logs = append(m.logs, msg)
		return m, handleDatagram(m)

	case errMsg:
		m.err = msg
		return m, tea.Quit

	case tea.WindowSizeMsg:
		m.terminalWidth = msg.Width
		m.terminalHeight = msg.Height
		return m, nil

	case tea.KeyPressMsg:
		if msg.Mod == tea.ModCtrl && msg.Code == 'c' {
			return m, tea.Quit
		}
	}

	return m, nil
}

func Start() error {
	c, err := net.ListenPacket("udp", "0.0.0.0:20081")
	if err != nil {
		log.Fatalf("could not start server connection: %s", err)
	}
	if _, err := tea.NewProgram(model{
		conn: c,
	}).Run(); err != nil {
		return err
	}

	return nil
}
