// Package decoder best pkg btw
package decoder

import (
	"bytes"
	"encoding/binary"
	"fmt"
)

type Header struct {
	Magic      [4]byte
	Sequence   uint32
	Timestamp  uint64
	Severity   uint8
	ItemID     uint16
	PayloadLen uint16
}

type Log struct {
	Header  Header
	Payload []byte
}

// Decode hi bro
func Decode(datagram []byte) (Log, error) {
	var l Log

	magicBytes := []byte{'F', 'C', 'G', 'O'}
	if !bytes.Equal(datagram[:4], magicBytes) {
		return Log{}, fmt.Errorf("invalid magic: %v does not match %v", datagram[:4], magicBytes)
	}
	copy(datagram[:4], l.Header.Magic[:])

	// sequence
	n, err := binary.Decode(datagram[4:8], binary.LittleEndian, &l.Header.Sequence)
	if err != nil {
		return Log{}, fmt.Errorf("decoding sequence: %w", err)
	}
	if n != binary.Size(l.Header.Sequence) {
		return Log{}, fmt.Errorf("expected %v wrote %v bytes", binary.Size(l.Header.Sequence), n)
	}

	// timestamp
	n, err = binary.Decode(datagram[8:16], binary.LittleEndian, &l.Header.Timestamp)
	if err != nil {
		return Log{}, fmt.Errorf("decoding timestamp: %w", err)
	}
	if n != binary.Size(l.Header.Timestamp) {
		return Log{}, fmt.Errorf("expected %v wrote %v bytes", binary.Size(l.Header.Timestamp), n)
	}

	// severity
	l.Header.Severity = datagram[16]

	// item id
	n, err = binary.Decode(datagram[17:19], binary.LittleEndian, &l.Header.ItemID)
	if err != nil {
		return Log{}, fmt.Errorf("decoding timestamp: %w", err)
	}
	if n != binary.Size(l.Header.ItemID) {
		return Log{}, fmt.Errorf("expected %v wrote %v bytes", binary.Size(l.Header.ItemID), n)
	}

	// payload length
	n, err = binary.Decode(datagram[19:21], binary.LittleEndian, &l.Header.PayloadLen)
	if err != nil {
		return Log{}, fmt.Errorf("decoding timestamp: %w", err)
	}
	if n != binary.Size(l.Header.PayloadLen) {
		return Log{}, fmt.Errorf("expected %v wrote %v bytes", binary.Size(l.Header.PayloadLen), n)
	}

	l.Payload = datagram[21:]

	return l, nil
}
