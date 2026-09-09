package main

import (
	"fmt"

	"charm.land/lipgloss/v2"
)

type Packet struct {
	Sequence   uint32
	ItemID     uint16
	PayloadLen uint16
	Payload    string
}

var (
	cardStyle = lipgloss.NewStyle().
			Border(lipgloss.ThickBorder()).
			Padding(0, 1)

	headerStyle = lipgloss.NewStyle().
			Bold(true)

	payloadStyle = lipgloss.NewStyle()
)

func renderPacketCard(p Packet) string {
	header := fmt.Sprintf(
		"#%d  item:%d  len:%d",
		p.Sequence,
		p.ItemID,
		p.PayloadLen,
	)

	body := p.Payload

	content := lipgloss.JoinVertical(
		lipgloss.Left,
		headerStyle.Render(header),
		payloadStyle.Render(body),
	)

	return cardStyle.Render(content)
}

func main() {
	p := Packet{
		Sequence:   42,
		ItemID:     901,
		PayloadLen: 19,
		Payload:    "random word: hotfix",
	}

	fmt.Println(renderPacketCard(p))
}
