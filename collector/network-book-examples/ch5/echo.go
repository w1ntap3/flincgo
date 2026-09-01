package echo

import (
	"context"
	"fmt"
	"net"
)

// INFO: you can check out a more comprehensive guide here;
// https://github.com/kdiffin/go-echo-server

// pseudo code
// bind to the udp socket, get its packetconn
// handle its error
// write a goroutine which has a cancellation way inside of it, usually go func() { <- ctx.Done() s.Close() }
// that goroutine should s.ReadFrom(buf) (read from the socket and write into the memory buffer)
// s.ReadFrom(buf) returns the amount of bits it wrote, the address of the client who sent the message, and an error
// after we handle the error, using the address of the client, s.WriteTo(memory, clientaddress) send the client a message.
// handle the error, finish the go func.

// context is for cancellgation, the nested goroutine blocks listening for ctx.done.
// if context.done, closes the socket.
// Ending its goroutine, if the socket is closed, the other goroutine dies as well.
// because the other goroutine is blocked doing s.Readfrom continously. Once the socket is closed, it errors, returning, ending the goroutines execution.
// when s writes to the buffer, the loop continues once more as s can readfrom buf
func echoServerUDP(ctx context.Context, addr string) (net.Addr, error) {
	// net.ListenPacket returns the an object with the interface net.PacketConn, which can be seen here https://pkg.go.dev/net#PacketConn
	// basically you can readfrom the specified network and address and gives you a way to readfrom and writeto.
	s, err := net.ListenPacket("udp", addr)
	if err != nil {
		return nil, fmt.Errorf("binding to udp %s: %w", addr, err)
	}

	go func() {
		go func() {
			<-ctx.Done()
			_ = s.Close()
		}()

		buf := make([]byte, 1024)
		for {
			n, clientAddr, err := s.ReadFrom(buf)
			fmt.Printf("read message: %s from clientAddr: %s\n", string(buf[:n]), clientAddr.String())
			if err != nil {
				return
			}

			// the reason why we do socket.WriteTo here is because we want the server to know its us, its an echo server.
			// that's why we're sending it via thsi socket, we couldve sent it through another socket without a problem.
			// **this is not a stateful connection like tcp**
			response := fmt.Sprintf("%s: %s", s.LocalAddr().String(), string(buf[:n]))
			_, err = s.WriteTo([]byte(response), clientAddr)
			if err != nil {
				return
			}
		}
	}()

	return s.LocalAddr(), nil
}
