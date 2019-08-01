/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNKCACHE_H_
#define CHUNKCACHE_H_

#include "./chunk.h"
#include "./chunkcachetypes.h"

#include <QObject>
#include <QCache>
#include <QSharedPointer>

template<typename EnumT, typename UnderlyingIntT>
class Bitset
{
public:
    Bitset()
        : m_raw(0)
    {}

    bool operator[](EnumT position) const
    {
        return test(position);
    }

    Bitset& operator<<(EnumT position)
    {
        set(position);
        return *this;
    }

    void unset(EnumT position)
    {
        m_raw = m_raw & ~(1 << (int)position);
    }

    void set(EnumT position)
    {
        m_raw = m_raw | (1 << (int)position);
    }

    bool test(EnumT position) const
    {
        return (m_raw & (1 << (int)position)) != 0;
    }

private:
    UnderlyingIntT m_raw;
};

class ChunkLoaderThreadPool;


class ChunkCache : public QObject {
  Q_OBJECT

 public:
  ChunkCache();
  ~ChunkCache();
  void clear();
  void setPath(QString path);
  QString getPath();

  enum class FetchBehaviour
  {
      USE_CACHED,
      USE_CACHED_OR_UDPATE,
      FORCE_UPDATE
  };

  class Locker
  {
  public:
      Locker(ChunkCache& parent)
          : m_parent(parent)
          , m_locker(&parent.mutex)
      {}

      bool isLoaded(int x, int z, QSharedPointer<Chunk> &chunkPtr_out)
      {
          return m_parent.isLoaded_unprotected(x,z,chunkPtr_out);
      }

      bool isCached(int x, int z, QSharedPointer<Chunk> &chunkPtr_out)
      {
          return m_parent.isCached_unprotected(x,z,chunkPtr_out);
      }

      bool fetch(QSharedPointer<Chunk>& chunk_out, int x, int z, FetchBehaviour behav = FetchBehaviour::USE_CACHED_OR_UDPATE)
      {
          return m_parent.fetch_unprotected(chunk_out, x, z, behav);
      }

  private:
      ChunkCache& m_parent;
      QMutexLocker m_locker;
  };

 signals:
  void chunkLoaded(const QSharedPointer<Chunk>& chunk, int x, int z);
  void structureFound(QSharedPointer<GeneratedStructure> structure);

 public slots:
  void adaptCacheToWindow(int x, int y);

 private slots:
  void gotChunk(const QSharedPointer<Chunk>& chunk, ChunkID id);
  void routeStructure(QSharedPointer<GeneratedStructure> structure);

 private:
  QString path;

  enum class ChunkState {
      Loading,
      Cached
  };

  struct ChunkInfoT
  {
    Bitset<ChunkState, uint8_t> state;
    QSharedPointer<Chunk> chunk;
  };

  QMap<ChunkID, ChunkInfoT> cachemap;
  QMutex mutex;
  int maxcache;
  QSharedPointer<ChunkLoaderThreadPool> m_loaderPool;

  void loadChunkAsync_unprotected(ChunkID id);

  bool isLoaded_unprotected(int x, int z, QSharedPointer<Chunk> &chunkPtr_out);

  bool isCached_unprotected(int x, int z, QSharedPointer<Chunk> &chunkPtr_out);

  bool fetch_unprotected(QSharedPointer<Chunk> &chunk_out, int x, int z, FetchBehaviour behav = FetchBehaviour::USE_CACHED_OR_UDPATE);

};

#endif  // CHUNKCACHE_H_
