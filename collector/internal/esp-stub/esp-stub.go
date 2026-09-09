// Package espstub this is a mock esp i use for development and debugging. sends an unencrypted copy of the c struct
package espstub

import (
	"encoding/binary"
	"fmt"
	"math/rand/v2"
	mrand "math/rand/v2"
	"net"
	"time"
)

func MockEdge(addr string) error {
	// hardcoded for now
	s, err := net.Dial("udp", addr)
	if err != nil {
		return fmt.Errorf("connecting to addr %s: %w", addr, err)
	}

	go func() {
		for {
			time.Sleep(time.Millisecond * 500)
			stub := stubDatagram()
			_, err = s.Write(stub)
			if err != nil {
				return
			}
		}
	}()

	return nil
}

var counter uint32

func stubDatagram() []byte {
	payload := []byte("random word: " + randomWord())
	const headerSize = 21
	buf := make([]byte, headerSize+len(payload))

	copy(buf[0:4], []byte{'F', 'C', 'G', 'O'})

	// AI GENERATED:
	// TODO: understand the binary lib better
	// Sequence number: uniquely identifies/orders this message.
	binary.LittleEndian.PutUint32(buf[4:8], counter)
	counter++

	// Timestamp: when the message was created.
	now := time.Now().Unix()
	randomSeconds := rand.Int64N(now)
	binary.LittleEndian.PutUint64(buf[8:16], uint64(randomSeconds))

	// Severity: single byte, so no byte-order conversion is needed.
	buf[16] = byte(rand.UintN(3))

	// Item ID: identifies the type/source of the logged item.
	binary.LittleEndian.PutUint16(buf[17:19], uint16(mrand.UintN(1000)))

	// Payload length: tells the decoder how many payload bytes follow the header.
	binary.LittleEndian.PutUint16(buf[19:21], uint16(len(payload)))

	copy(buf[21:], payload)

	return buf
}

var words = [...]string{
	"segfault",
	"spaghetti",
	"goku",
	"broly",
	"rogue lineage",
	"pikachu",
	"yugioh",
	"deadlock",
	"oom",
	"forkbomb",
	"goblin",
	"kernelpanic",
	"racecondition",
	"techdebt",
	"works-on-my-machine",
	"undefined-behavior",
	"sudo",
	"daemon",
	"localhost",
	"nullpointer",
	"yolo-deploy",
	"hotfix",
	"legacy",
	"microservice",
	"monad",
	"regex",
	"kubectl",
	"rm-rf",
	"off-by-one",
}

func randomWord() string {
	return words[mrand.IntN(len(words))]
}
