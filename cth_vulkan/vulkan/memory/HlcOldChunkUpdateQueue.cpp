#include "HlcChunkUpdateQueue.hpp"


namespace cth {

void ChunkUpdateQueue::queueVerticesUpdate(const uint32_t first_vertex, const uint32_t update_size) {
	queueUpdate(first_vertex, update_size, vertexQueue, CHUNK_UPDATE_QUEUE_TOLERANCE);
}
void ChunkUpdateQueue::queueIndicesUpdate(const uint32_t first_index, const uint32_t update_size) {
	queueUpdate(first_index, update_size, indexQueue, CHUNK_UPDATE_QUEUE_TOLERANCE);
}

void ChunkUpdateQueue::queueUpdate(const uint32_t first, const uint32_t size, vector<uint32_t>& queue, const uint32_t tolerance = CHUNK_UPDATE_QUEUE_TOLERANCE) {
	assert(tolerance > 0 && "tolerance < 1 is undefined");
	assert(size != 0 && "queued chunk update must be > 0");

	const uint32_t last = first + size;

	const size_t queueFirst = algorithm::sorted::findIndexFor(first > tolerance ? first - tolerance : 0, queue);
	const size_t queueLast = algorithm::sorted::findIndexFor(last + tolerance, queue);

	const bool firstOdd = queueFirst & 0x1, secondOdd = queueLast & 0x1;

	if(!firstOdd && !secondOdd) {
		queue.insert(queue.begin() + queueLast, last);
		queue.insert(queue.begin() + queueFirst, first);

		if(queueFirst != queueLast) queue.erase(queue.begin() + queueFirst + 1, queue.begin() + queueLast + 1);
	}
	else if(!firstOdd && secondOdd) {
		queue.erase(queue.begin() + queueFirst, queue.begin() + queueLast);
		queue.insert(queue.begin() + queueFirst, first);
	}
	else if(firstOdd && !secondOdd) {
		queue.insert(queue.begin() + queueLast, last);
		queue.erase(queue.begin() + queueFirst, queue.begin() + queueLast);
	}
	else if(firstOdd && queueFirst != queueLast && secondOdd) queue.erase(queue.begin() + queueFirst, queue.begin() + queueLast);

	assert(!(queue.size() & 0x1) && "queue size cant be odd");
}
}