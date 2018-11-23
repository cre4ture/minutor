/** Copyright (c) 2013, Sean Kasun */

#include "./chunkcache.h"
#include "./chunkloader.h"

#if defined(__unix__) || defined(__unix) || defined(unix)
#include <unistd.h>
#elif defined(_WIN32) || defined(WIN32)
#include <windows.h>
#endif

ChunkID::ChunkID(int x, int z) : x(x), z(z) {
}
bool ChunkID::operator==(const ChunkID &other) const {
    return (other.x == x) && (other.z == z);
}

bool ChunkID::operator<(const ChunkID &other) const
{
    return (other.x < x) || ((other.x == x) && (other.z < z));
}
uint qHash(const ChunkID &c) {
  return (c.x << 16) ^ (c.z & 0xffff);  // safe way to hash a pair of integers
}

ChunkCache::ChunkCache() {
  int chunks = 10000;  // 10% more than 1920x1200 blocks
#if defined(__unix__) || defined(__unix) || defined(unix)
#ifdef _SC_AVPHYS_PAGES
  auto pages = sysconf(_SC_AVPHYS_PAGES);
  auto page_size = sysconf(_SC_PAGE_SIZE);
  chunks = (pages*page_size) / (sizeof(Chunk) + 16*sizeof(ChunkSection));
#endif
#elif defined(_WIN32) || defined(WIN32)
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  DWORDLONG available = qMin(status.ullAvailPhys, status.ullAvailVirtual);
  chunks = available / (sizeof(Chunk) + 16 * sizeof(ChunkSection));
#endif
  //cache.setMaxCost(chunks);
  maxcache = 2 * chunks;  // most chunks are less than half filled with sections
}

ChunkCache::~ChunkCache() {
}

void ChunkCache::clear() {
  QThreadPool::globalInstance()->waitForDone();
  mutex.lock();
  cache.clear();
  mutex.unlock();
}

void ChunkCache::setPath(QString path) {
  this->path = path;
}
QString ChunkCache::getPath() {
  return path;
}

Chunk *ChunkCache::fetch(int x, int z)
{
  ChunkID id(x, z);
  mutex.lock();
  Chunk *chunk = cache[id];
  mutex.unlock();
  if (chunk != NULL) {
    if (chunk->loaded)
      return chunk;
    return NULL;  // we're loading this chunk, or it's blank.
  }
  // launch background process to load this chunk
  chunk = new Chunk();
  mutex.lock();
  cache.insert(id, chunk);
  mutex.unlock();
  ChunkLoader *loader = new ChunkLoader(path, x, z, cache, &mutex);
  connect(loader, SIGNAL(chunkUpdated(bool, int, int)),
          this, SLOT(gotChunk(bool, int, int)));
  QThreadPool::globalInstance()->start(loader);
  return NULL;
}

bool ChunkCache::isLoaded(int x, int z, Chunk *&chunkPtr_out)
{
    ChunkID id(x, z);
    mutex.lock();
    chunkPtr_out = cache[id];
    mutex.unlock();
    if (chunkPtr_out != NULL) {
      if (chunkPtr_out->loaded)
        return true;
    }

    chunkPtr_out = nullptr;
    return false;  // we're loading this chunk, or it's blank.
}

void ChunkCache::gotChunk(bool success, int x, int z) {
  emit chunkLoaded(success, x, z);
}

void ChunkCache::adaptCacheToWindow(int x, int y) {
  int chunks = ((x + 15) >> 4) * ((y + 15) >> 4);  // number of chunks visible
  chunks *= 1.10;  // add 10%
  //cache.setMaxCost(qMin(chunks, maxcache));
}
