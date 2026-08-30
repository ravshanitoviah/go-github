// buffer_pool.go
package bufferpool

import (
	"sync"
)

// BufferPool manages a pool of byte slices to prevent memory leaks
type BufferPool struct {
	pool sync.Pool
}

// NewBufferPool creates a new buffer pool with the specified buffer size
func NewBufferPool(size int) *BufferPool {
	return &BufferPool{
		pool: sync.Pool{
			New: func() interface{} {
				return make([]byte, size)
			},
		},
	}
}

// Get retrieves a buffer from the pool
func (bp *BufferPool) Get() []byte {
	return bp.pool.Get().([]byte)
}

// Put returns a buffer to the pool
func (bp *BufferPool) Put(buf []byte) {
	// Reset buffer to avoid data leaks between uses
	for i := range buf {
		buf[i] = 0
	}
	bp.pool.Put(buf)
}

// Buffer represents a managed buffer with automatic return to pool
type Buffer struct {
	pool *BufferPool
	data []byte
}

// NewBuffer creates a new managed buffer from the pool
func (bp *BufferPool) NewBuffer() *Buffer {
	return &Buffer{
		pool: bp,
		data: bp.Get(),
	}
}

// Data returns the underlying byte slice
func (b *Buffer) Data() []byte {
	return b.data
}

// Close returns the buffer to the pool and invalidates the buffer
func (b *Buffer) Close() error {
	if b.data != nil {
		b.pool.Put(b.data)
		b.data = nil
	}
	return nil
}

// BufferManager manages multiple buffer pools and tracks active buffers
type BufferManager struct {
	pool       *BufferPool
	buffers    map[*Buffer]struct{}
	buffersMux sync.Mutex
}

// NewBufferManager creates a new buffer manager with the specified buffer size
func NewBufferManager(size int) *BufferManager {
	return &BufferManager{
		pool:    NewBufferPool(size),
		buffers: make(map[*Buffer]struct{}),
	}
}

// GetBuffer gets a new buffer and tracks it
func (bm *BufferManager) GetBuffer() *Buffer {
	buf := bm.pool.NewBuffer()
	bm.buffersMux.Lock()
	bm.buffers[buf] = struct{}{}
	bm.buffersMux.Unlock()
	return buf
}

// ReleaseBuffer removes a buffer from tracking and returns it to pool
func (bm *BufferManager) ReleaseBuffer(buf *Buffer) error {
	bm.buffersMux.Lock()
	delete(bm.buffers, buf)
	bm.buffersMux.Unlock()
	return buf.Close()
}

// Close releases all tracked buffers and shuts down the manager
func (bm *BufferManager) Close() error {
	bm.buffersMux.Lock()
	defer bm.buffersMux.Unlock()
	
	for buf := range bm.buffers {
		buf.Close()
	}
	bm.buffers = make(map[*Buffer]struct{})
	return nil
}