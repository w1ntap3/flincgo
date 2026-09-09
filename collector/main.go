package main

import (
	"log"

	espstub "github.com/w1ntap3/flincgo/collector/internal/esp-stub"
	"github.com/w1ntap3/flincgo/collector/internal/tui"
)

func main() {
	// this is for testing. sends random logs in our format via udp
	err := espstub.MockEdge("0.0.0.0:20081")
	if err != nil {
		log.Printf("starting mock edge: %s", err)
		return
	}

	// this starts the tui
	err = tui.Start()
	if err != nil {
		log.Fatalf("starting tui: %s", err)
	}
}
