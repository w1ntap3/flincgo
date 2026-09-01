package echo

import (
	"bytes"
	"context"
	"net"
	"testing"
)

func TestListenPacketUDP(t *testing.T) {
	ctx, cancel := context.WithCancel(context.Background())
	serverAddr, err := echoServerUDP(ctx, "127.0.0.1:")
	if err != nil {
		t.Fatal(err)
	}
	defer cancel()

	client, err := net.ListenPacket("udp", "127.0.0.1:")
	if err != nil {
		t.Fatal(err)
	}
	defer func() { _ = client.Close() }()

	interloper, err := net.ListenPacket("udp", "127.0.0.1:")
	if err != nil {
		t.Fatal(err)
	}

	interrupt := []byte("pardon me")
	n, err := interloper.WriteTo(interrupt, client.LocalAddr())
	if err != nil {
		t.Fatal(err)
	}
	_ = interloper.Close()

	if l := len(interrupt); l != n {
		t.Fatalf("wrote %d bytes of %d", n, l) // very low lvl bug where the kernel supposedly doesnt write the message you wanted to write
	}

	ping := []byte("ping")
	_, err = client.WriteTo(ping, serverAddr)

	buf := make([]byte, 1024)
	n, addr, err := client.ReadFrom(buf)
	if err != nil {
		t.Fatal(err)
	}

	if !bytes.Equal(interrupt, buf[:n]) {
		t.Errorf("expected reply %q; actual reply %q", interrupt, buf[:n])
	}
	if addr.String() != interloper.LocalAddr().String() {
		t.Errorf("expected message from %q; actual sender is %q", interloper.LocalAddr(), addr)
	}
	n, addr, err = client.ReadFrom(buf)
	if err != nil {
		t.Fatal(err)
	}
	if !bytes.Equal(ping, buf[:n]) {
		t.Errorf("expected reply %q; actual reply %q", ping, buf[:n])
	}
	if addr.String() != serverAddr.String() {
		t.Errorf("expected message from %q; actual sender is %q", serverAddr, addr)
	}
}
