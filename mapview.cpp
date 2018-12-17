/** Copyright (c) 2013, Sean Kasun */

#include "./mapview.h"
#include "./definitionmanager.h"
#include "./blockidentifier.h"
#include "./biomeidentifier.h"
#include "./clamp.h"
#include "./chunkrenderer.h"

#include <QObject>
#include <QRunnable>
#include <QPainter>
#include <QResizeEvent>
#include <QMessageBox>
#include <assert.h>

const double g_zoomMin = 0.25;
const double g_zoomMax = 20.0;

class DrawHelper
{
private:
    MapView& parent;

public:
    double& zoom;

    int startx; // first chunk left
    int startz; // first chunk top

    int blockswide; // width in chunks
    int blockstall; // height in chunks

    double x1; // first coordinate left
    double z1; // first coordinate top
    double x2;
    double z2;

    DrawHelper(MapView& parent_)
        : parent(parent_)
        , zoom(parent.zoom)
    {
        auto& image = parent.image;
        auto& x = parent.x;
        auto& z = parent.z;

        double chunksize = 16 * zoom;

        // first find the center block position
        int centerchunkx = floor(x / 16);
        int centerchunkz = floor(z / 16);
        // and the center of the screen
        int centerx = image.width() / 2;
        int centery = image.height() / 2;
        // and align for panning
        centerx -= (x - centerchunkx * 16) * zoom;
        centery -= (z - centerchunkz * 16) * zoom;
        // now calculate the topleft block on the screen
        startx = centerchunkx - floor(centerx / chunksize) - 1;
        startz = centerchunkz - floor(centery / chunksize) - 1;
        // and the dimensions of the screen in blocks
        blockswide = image.width() / chunksize + 3;
        blockstall = image.height() / chunksize + 3;


        double halfviewwidth = image.width() / 2 / zoom;
        double halvviewheight = image.height() / 2 / zoom;
        x1 = x - halfviewwidth;
        z1 = z - halvviewheight;
        x2 = x + halfviewwidth;
        z2 = z + halvviewheight;
    }

    void drawPlayers(QPainter& canvas)
    {
        for (const auto& playerEntity: parent.currentPlayers)
        {
            playerEntity->draw(x1, z1, zoom, &canvas);
        }
    }
};

MapView::MapView(QWidget *parent) : QWidget(parent) {
  depth = 255;
  scale = 1;
  zoom = 1.0;
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);

  int offset = 0;
  for (int y = 0; y < 16; y++)
    for (int x = 0; x < 16; x++) {
      uchar color = ((x & 8) ^ (y & 8)) == 0 ? 0x44 : 0x88;
      placeholder[offset++] = color;
      placeholder[offset++] = color;
      placeholder[offset++] = color;
      placeholder[offset++] = 0xff;
    }
  // calculate exponential function for cave shade
  float cavesum = 0.0;
  for (int i=0; i<CAVE_DEPTH; i++) {
    caveshade[i] = 1/exp(i/(CAVE_DEPTH/2.0));
    cavesum += caveshade[i];
  }
  for (int i=0; i<CAVE_DEPTH; i++) {
    caveshade[i] = 1.5 * caveshade[i] / cavesum;
  }
}

QSize MapView::minimumSizeHint() const {
  return QSize(300, 300);
}
QSize MapView::sizeHint() const {
  return QSize(400, 400);
}

void MapView::attach(DefinitionManager *dm) {
  this->dm = dm;
  connect(dm, SIGNAL(packsChanged()),
          this, SLOT(redraw()));
  this->blockDefinitions = dm->blockIdentifier();
  this->biomes = dm->biomeIdentifier();
}

void MapView::attach(QSharedPointer<ChunkCache> chunkCache_)
{
    cache = chunkCache_;

    connect(cache.get(), SIGNAL(chunkLoaded(bool, int, int)),
            this, SLOT(chunkUpdated(bool, int, int)));
}

void MapView::setLocation(double x, double z) {
  setLocation(x, depth, z, false, true);
}

void MapView::setLocation(double x, int y, double z, bool ignoreScale, bool useHeight) {
  this->x = ignoreScale ? x : x / scale;
  this->z = ignoreScale ? z : z / scale;
  if (useHeight == true && depth != y) {
    emit demandDepthValue(y);
  } else {
    redraw();
  }
}

MapView::BlockLocation *MapView::getLocation()
{
  currentLocation.x = x;
  currentLocation.y = depth;
  currentLocation.z = z;
  currentLocation.scale = scale;

  return &currentLocation;
}

void MapView::setDimension(QString path, int scale) {
  if (scale > 0) {
    this->x *= this->scale;
    this->z *= this->scale;  // undo current scale transform
    this->scale = scale;
    this->x /= scale;  // and do the new scale transform
    this->z /= scale;
  } else {
    this->scale = 1;  // no scaling because no relation to overworld
    this->x = 0;  // and we jump to the center spawn automatically
    this->z = 0;
  }
  cache->clear();
  cache->setPath(path);
  redraw();
}

void MapView::setDepth(int depth) {
  this->depth = depth;
  redraw();
}

void MapView::setFlags(int flags) {
  this->flags = flags;
}

void MapView::chunkUpdated(bool, int x, int z) {
  drawChunk(x, z);
  DrawHelper h(*this);
  QPainter canvas(&image);
  h.drawPlayers(canvas);
  update();
}

QString MapView::getWorldPath() {
    return cache->getPath();
}

void MapView::updatePlayerPositions(const QVector<PlayerInfo> &playerList)
{
    currentPlayers.clear();
    for (auto info: playerList)
    {
        auto entity = QSharedPointer<Entity>::create(info);
        currentPlayers.push_back(entity);
    }

    DrawHelper h(*this);
    QPainter canvas(&image);
    h.drawPlayers(canvas);
}

void MapView::clearCache() {
  cache->clear();
  redraw();
}

void MapView::mousePressEvent(QMouseEvent *event) {
  lastMousePressPosition = event->pos();
  dragging = true;
}

MapView::TopViewPosition MapView::transformMousePos(QPoint mouse_pos)
{
    double centerblockx = floor(this->x);
    double centerblockz = floor(this->z);

    int centerx = image.width() / 2;
    int centery = image.height() / 2;

    centerx -= (this->x - centerblockx) * zoom;
    centery -= (this->z - centerblockz) * zoom;

    return TopViewPosition(
                floor(centerblockx - (centerx - mouse_pos.x()) / zoom),
                floor(centerblockz - (centery - mouse_pos.y()) / zoom)
                );
}

void MapView::renderChunkAsync(const QSharedPointer<Chunk> &chunk)
{
    ChunkID id(chunk->chunkX, chunk->chunkZ);

    {
        QMutexLocker locker(&m_renderStatesMutex);

        auto state = renderStates[id];

        if (state == RENDERING)
        {
            return; // already busy with this chunk
        }

        renderStates[id] = RENDERING;
    }

    ChunkRenderer *loader = new ChunkRenderer(chunk, *this);
    connect(loader, SIGNAL(chunkRenderingCompleted(QSharedPointer<Chunk>)),
            this, SLOT(renderingDone(const QSharedPointer<Chunk>&)));
    QThreadPool::globalInstance()->start(loader);
}

void MapView::renderingDone(const QSharedPointer<Chunk> &chunk)
{
    ChunkID id(chunk->chunkX, chunk->chunkZ);
    {
        QMutexLocker locker(&m_renderStatesMutex);
        renderStates[id] = NONE;
    }

    drawChunk(chunk->chunkX, chunk->chunkZ);

    DrawHelper h(*this);
    QPainter canvas(&image);
    h.drawPlayers(canvas);
}

void MapView::getToolTipMousePos(int mouse_x, int mouse_y)
{
    TopViewPosition worldPos =  transformMousePos(QPoint(mouse_x, mouse_y));

    getToolTip(worldPos.x, worldPos.z);
}


void MapView::mouseMoveEvent(QMouseEvent *event) {
  if (!dragging) {
    return;
  }
  x += (lastMousePressPosition.x()-event->x()) / zoom;
  z += (lastMousePressPosition.y()-event->y()) / zoom;
  lastMousePressPosition = event->pos();

  redraw();
}

void MapView::mouseReleaseEvent(QMouseEvent * event) {
  dragging = false;

  if (event->pos() == lastMousePressPosition)
  {
      // no movement of cursor -> assume normal click:
      getToolTipMousePos(event->x(), event->y());
  }
}

void MapView::mouseDoubleClickEvent(QMouseEvent *event) {
  int centerblockx = floor(this->x);
  int centerblockz = floor(this->z);

  int centerx = image.width() / 2;
  int centery = image.height() / 2;

  centerx -= (this->x - centerblockx) * zoom;
  centery -= (this->z - centerblockz) * zoom;

  int mx = floor(centerblockx - (centerx - event->x()) / zoom);
  int mz = floor(centerblockz - (centery - event->y()) / zoom);

  // get the y coordinate
  int my = getY(mx, mz);

  QList<QVariant> properties;
  for (auto &item : getItems(mx, my, mz)) {
    properties.append(item->properties());
  }

  if (!properties.isEmpty()) {
    emit showProperties(properties);
  }
}

void MapView::wheelEvent(QWheelEvent *event) {
  if ((event->modifiers() & Qt::ShiftModifier) == Qt::ShiftModifier) {
    // change depth
    emit demandDepthChange(event->delta() / 120);
  } else {  // change zoom
    zoom += floor(event->delta() / 90.0);
    if (zoom < g_zoomMin) zoom = g_zoomMin;
    if (zoom > g_zoomMax) zoom = g_zoomMax;
    redraw();
  }
}

void MapView::keyPressEvent(QKeyEvent *event) {
  // default: 16 blocks / 1 chunk
  float stepSize = 16.0;

  if ((event->modifiers() & Qt::ShiftModifier) == Qt::ShiftModifier) {
    // 1 block for fine tuning
    stepSize = 1.0;
  }
  else if ((event->modifiers() & Qt::AltModifier) == Qt::AltModifier) {
    // 8 chunks
    stepSize = 128.0;
    if ((event->modifiers() & Qt::ControlModifier) == Qt::ControlModifier) {
      // 32 chunks / 1 Region
      stepSize = 512.0;
    }
  }

  switch (event->key()) {
    case Qt::Key_Up:
    case Qt::Key_W:
      z -= stepSize / zoom;
      redraw();
      break;
    case Qt::Key_Down:
    case Qt::Key_S:
      z += stepSize / zoom;
      redraw();
      break;
    case Qt::Key_Left:
    case Qt::Key_A:
      x -= stepSize / zoom;
      redraw();
      break;
    case Qt::Key_Right:
    case Qt::Key_D:
      x += stepSize / zoom;
      redraw();
      break;
    case Qt::Key_PageUp:
    case Qt::Key_Q:
      zoom++;
      if (zoom > g_zoomMax) zoom = g_zoomMax;
      redraw();
      break;
    case Qt::Key_PageDown:
    case Qt::Key_E:
      zoom--;
      if (zoom < g_zoomMin) zoom = g_zoomMin;
      redraw();
      break;
    case Qt::Key_Home:
    case Qt::Key_Plus:
    case Qt::Key_BracketLeft:
      emit demandDepthChange(+1);
      break;
    case Qt::Key_End:
    case Qt::Key_Minus:
    case Qt::Key_BracketRight:
      emit demandDepthChange(-1);
      break;
  }
}

void MapView::resizeEvent(QResizeEvent *event) {
  image = QImage(event->size(), QImage::Format_RGB32);
  redraw();
}

void MapView::paintEvent(QPaintEvent * /* event */) {
  QPainter p(this);
  p.drawImage(QPoint(0, 0), image);
  p.end();
}

void MapView::redraw() {
  if (!this->isEnabled()) {
    // blank
    uchar *bits = image.bits();
    int imgstride = image.bytesPerLine();
    int imgoffset = 0;
    for (int y = 0; y < image.height(); y++, imgoffset += imgstride)
      memset(bits + imgoffset, 0xee, imgstride);
    update();
    return;
  }

  DrawHelper h(*this);

  for (int cz = h.startz; cz < h.startz + h.blockstall; cz++)
    for (int cx = h.startx; cx < h.startx + h.blockswide; cx++)
      drawChunk(cx, cz);

  // add on the entity layer
  QPainter canvas(&image);

  // draw the entities
  for (int cz = h.startz; cz < h.startz + h.blockstall; cz++) {
    for (int cx = h.startx; cx < h.startx + h.blockswide; cx++) {
      for (auto &type : overlayItemTypes) {
        auto chunk = cache->fetch(cx, cz);
        if (chunk) {
          auto range = chunk->entities.equal_range(type);
          for (auto it = range.first; it != range.second; ++it) {
            // don't show entities above our depth
            int entityY = (*it)->midpoint().y;
            // everything below the current block,
            // but also inside the current block
            if (entityY < depth + 1) {
              int entityX = static_cast<int>((*it)->midpoint().x) & 0x0f;
              int entityZ = static_cast<int>((*it)->midpoint().z) & 0x0f;
              int index = entityX + (entityZ << 4);
              int highY = chunk->depth[index];
              if ( (entityY+10 >= highY) ||
                   (entityY+10 >= depth) )
                (*it)->draw(h.x1, h.z1, zoom, &canvas);
            }
          }
        }
      }
    }
  }

  // draw the generated structures
  for (auto &type : overlayItemTypes) {
    for (auto &item : overlayItems[type]) {
      if (item->intersects(OverlayItem::Point(h.x1 - 1, 0, h.z1 - 1),
                           OverlayItem::Point(h.x2 + 1, depth, h.z2 + 1))) {
        item->draw(h.x1, h.z1, zoom, &canvas);
      }
    }
  }

  const int maxViewWidth = 64 * 16; // (radius 32 chunks)

  const double firstGridLineX = ceil(h.x1 / maxViewWidth) * maxViewWidth;
  for (double x = firstGridLineX; x < h.x2; x += maxViewWidth)
  {
      const int line_x = round((x - h.x1) * zoom);
      canvas.drawLine(line_x, 0, line_x, image.height());
  }

  const double firstGridLineZ = ceil(h.z1 / maxViewWidth) * maxViewWidth;
  for (double z = firstGridLineZ; z < h.z2; z += maxViewWidth)
  {
      const int line_z = round((z - h.z1) * zoom);
      canvas.drawLine(0, line_z, image.width(), line_z);
  }

  h.drawPlayers(canvas);

  emit(coordinatesChanged(x, depth, z));

  update();
}

void MapView::drawChunk(int x, int z) {
  if (!this->isEnabled())
    return;

  uchar *src = placeholder;
  // fetch the chunk
  auto chunk = cache->fetch(x, z);

  if (chunk && (chunk->renderedAt != depth ||
                chunk->renderedFlags != flags)) {
    renderChunkAsync(chunk);
    return;
  }

  // this figures out where on the screen this chunk should be drawn

  // first find the center chunk
  int centerchunkx = floor(this->x / 16);
  int centerchunkz = floor(this->z / 16);
  // and the center chunk screen coordinates
  int centerx = image.width() / 2;
  int centery = image.height() / 2;
  // which need to be shifted to account for panning inside that chunk
  centerx -= (this->x - centerchunkx * 16) * zoom;
  centery -= (this->z - centerchunkz * 16) * zoom;
  // centerx,y now points to the top left corner of the center chunk
  // so now calculate our x,y in relation
  double chunksize = 16 * zoom;
  centerx += (x - centerchunkx) * chunksize;
  centery += (z - centerchunkz) * chunksize;

  int srcoffset = 0;
  uchar *bits = image.bits();
  int imgstride = image.bytesPerLine();

  int skipx = 0, skipy = 0;
  int blockwidth = chunksize, blockheight = chunksize;
  // now if we're off the screen we need to crop
  if (centerx < 0) {
    skipx = -centerx;
    centerx = 0;
  }
  if (centery < 0) {
    skipy = -centery;
    centery = 0;
  }
  // or the other side, we need to trim
  if (centerx + blockwidth > image.width())
    blockwidth = image.width() - centerx;
  if (centery + blockheight > image.height())
    blockheight = image.height() - centery;
  if (blockwidth <= 0 || skipx >= blockwidth) return;
  int imgoffset = centerx * 4 + centery * imgstride;
  if (chunk)
    src = chunk->image;
  // blit (or scale blit)
  for (int z = skipy; z < blockheight; z++, imgoffset += imgstride) {
    srcoffset = floor(z / zoom) * 16 * 4;
    if (zoom == 1.0) {
      memcpy(bits + imgoffset, src + srcoffset + skipx * 4,
             (blockwidth - skipx) * 4);
    } else {
      int xofs = 0;
      for (int x = skipx; x < blockwidth; x++, xofs += 4)
        memcpy(bits + imgoffset + xofs, src + srcoffset +
               static_cast<int>(floor(x / zoom) * 4), 4);
    }
  }
}

ChunkRenderer::ChunkRenderer(const QSharedPointer<Chunk> &chunk, MapView &parent_)
    : m_chunk(chunk)
    , parent(parent_)
{}

ChunkRenderer::~ChunkRenderer()
{}

void ChunkRenderer::run()
{
    renderChunk(m_chunk.get());
    emit chunkRenderingCompleted(m_chunk);
}

void ChunkRenderer::renderChunk(Chunk *chunk)
{
    int depth;
    int flags;

    {
        QReadLocker locker(&parent.m_readWriteLock);

        depth = parent.depth;
        flags = parent.flags;
    }

    auto& blocksDefinitions = parent.blockDefinitions;

  int offset = 0;
  uchar *bits = chunk->image;
  uchar *depthbits = chunk->depth;
  for (int z = 0; z < 16; z++) {  // n->s
    int lasty = -1;
    for (int x = 0; x < 16; x++, offset++) {  // e->w
      // initialize color
      uchar r = 0, g = 0, b = 0;
      double alpha = 0.0;
      // get Biome
      auto &biome = parent.biomes->getBiome(chunk->biomes[offset]);
      int top = depth;
      if (top > chunk->highest)
        top = chunk->highest;
      int highest = 0;
      for (int y = top; y >= 0; y--) {  // top->down
        int sec = y >> 4;
        ChunkSection *section = chunk->sections[sec];
        if (!section) {
          y = (sec << 4) - 1;  // skip whole section
          continue;
        }

        // get data value
        int data = section->getData(offset, y);

        // get BlockInfo from block value
        BlockInfo &block = blocksDefinitions->getBlock(section->getBlock(offset, y),
                                            data);
        if (block.alpha == 0.0) continue;

        // get light value from one block above
        int light = 0;
        ChunkSection *section1 = NULL;
        if (y < 255)
          section1 = chunk->sections[(y+1) >> 4];
        if (section1)
          light = section1->getLight(offset, y+1);
        int light1 = light;
        if (!(flags & MapView::flgLighting))
          light = 13;
        if (alpha == 0.0 && lasty != -1) {
          if (lasty < y)
            light += 2;
          else if (lasty > y)
            light -= 2;
        }
//        if (light < 0) light = 0;
//        if (light > 15) light = 15;

        // get current block color
        QColor blockcolor = block.colors[15];  // get the color from Block definition
        if (block.biomeWater()) {
          blockcolor = biome.getBiomeWaterColor(blockcolor);
        }
        else if (block.biomeGrass()) {
          blockcolor = biome.getBiomeGrassColor(blockcolor, y-64);
        }
        else if (block.biomeFoliage()) {
          blockcolor = biome.getBiomeFoliageColor(blockcolor, y-64);
        }

        // shade color based on light value
        double light_factor = pow(0.90,15-light);
        quint32 colr = std::clamp( int(light_factor*blockcolor.red()),   0, 255 );
        quint32 colg = std::clamp( int(light_factor*blockcolor.green()), 0, 255 );
        quint32 colb = std::clamp( int(light_factor*blockcolor.blue()),  0, 255 );

        // process flags
        if (flags & MapView::flgDepthShading) {
          // Use a table to define depth-relative shade:
          static const quint32 shadeTable[] = {
            0, 12, 18, 22, 24, 26, 28, 29, 30, 31, 32};
          size_t idx = qMin(static_cast<size_t>(depth - y),
                            sizeof(shadeTable) / sizeof(*shadeTable) - 1);
          quint32 shade = shadeTable[idx];
          colr = colr - qMin(shade, colr);
          colg = colg - qMin(shade, colg);
          colb = colb - qMin(shade, colb);
        }
        if (flags & MapView::flgMobSpawn) {
          // get block info from 1 and 2 above and 1 below
          quint16 blid1(0), blid2(0), blidB(0);  // default to air
          int data1(0), data2(0), dataB(0);  // default variant
          ChunkSection *section2 = NULL;
          ChunkSection *sectionB = NULL;
          if (y < 254)
            section2 = chunk->sections[(y+2) >> 4];
          if (y > 0)
            sectionB = chunk->sections[(y-1) >> 4];
          if (section1) {
            blid1 = section1->getBlock(offset, y+1);
            data1 = section1->getData(offset, y+1);
          }
          if (section2) {
            blid2 = section2->getBlock(offset, y+2);
            data2 = section2->getData(offset, y+2);
          }
          if (sectionB) {
            blidB = sectionB->getBlock(offset, y-1);
            dataB = sectionB->getData(offset, y-1);
          }
          BlockInfo &block2 = blocksDefinitions->getBlock(blid2, data2);
          BlockInfo &block1 = blocksDefinitions->getBlock(blid1, data1);
          BlockInfo &block0 = block;
          BlockInfo &blockB = blocksDefinitions->getBlock(blidB, dataB);
          int light0 = section->getLight(offset, y);

          // spawn check #1: on top of solid block
          if (block0.doesBlockHaveSolidTopSurface(data) &&
              !block0.isBedrock() && light1 < 8 &&
              !block1.isBlockNormalCube() && block1.spawninside &&
              !block1.isLiquid() &&
              !block2.isBlockNormalCube() && block2.spawninside) {
            colr = (colr + 256) / 2;
            colg = (colg + 0) / 2;
            colb = (colb + 192) / 2;
          }
          // spawn check #2: current block is transparent,
          // but mob can spawn through (e.g. snow)
          if (blockB.doesBlockHaveSolidTopSurface(dataB) &&
              !blockB.isBedrock() && light0 < 8 &&
              !block0.isBlockNormalCube() && block0.spawninside &&
              !block0.isLiquid() &&
              !block1.isBlockNormalCube() && block1.spawninside) {
            colr = (colr + 192) / 2;
            colg = (colg + 0) / 2;
            colb = (colb + 256) / 2;
          }
        }
        if (flags & MapView::flgBiomeColors) {
          colr = biome.colors[light].red();
          colg = biome.colors[light].green();
          colb = biome.colors[light].blue();
          alpha = 0;
        }

        // combine current block to final color
        if (alpha == 0.0) {
          // first color sample
          alpha = block.alpha;
          r = colr;
          g = colg;
          b = colb;
          highest = y;
        } else {
          // combine further color samples with blending
          r = (quint8)(alpha * r + (1.0 - alpha) * colr);
          g = (quint8)(alpha * g + (1.0 - alpha) * colg);
          b = (quint8)(alpha * b + (1.0 - alpha) * colb);
          alpha += block.alpha * (1.0 - alpha);
        }

        // finish depth (Y) scanning when color is saturated enough
        if (block.alpha == 1.0 || alpha > 0.9)
          break;
      }
      if (flags & MapView::flgCaveMode) {
        float cave_factor = 1.0;
        int cave_test = 0;
        for (int y=highest-1; (y >= 0) && (cave_test < MapView::CAVE_DEPTH); y--, cave_test++) {  // top->down
          // get section
          ChunkSection *section = chunk->sections[y >> 4];
          if (!section) continue;
          // get data value
          int data = section->getData(offset, y);
          // get BlockInfo from block value
          BlockInfo &block = blocksDefinitions->getBlock(section->getBlock(offset, y), data);
          if (block.transparent) {
            cave_factor -= parent.caveshade[cave_test];
          }
        }
        cave_factor = std::max(cave_factor,0.25f);
        // darken color by blending with cave shade factor
        r = (quint8)(cave_factor * r);
        g = (quint8)(cave_factor * g);
        b = (quint8)(cave_factor * b);
      }
      *depthbits++ = lasty = highest;
      *bits++ = b;
      *bits++ = g;
      *bits++ = r;
      *bits++ = 0xff;
    }
  }
  chunk->renderedAt = depth;
  chunk->renderedFlags = flags;
}

void MapView::getToolTip(int x, int z) {
  int cx = floor(x / 16.0);
  int cz = floor(z / 16.0);
  auto chunk = cache->fetch(cx, cz);
  Block block;

  QString name = "Unknown";
  QString biome = "Unknown Biome";
  QMap<QString, int> entityIds;

  if (chunk) {
    int top = qMin(depth, chunk->highest);
    int y = 0;
    for (y = top; y >= 0; y--) {
      int sec = y >> 4;
      {
          ChunkSection *section = chunk->sections[sec];
          if (!section) {
            y = (sec << 4) - 1;  // skip entire section
            continue;
          }
      }

      block = chunk->getBlockData(x,y,z);

      auto &blockInfo = blockDefinitions->getBlock(block.id, block.bd);
      if (blockInfo.alpha == 0.0) continue;
      // found block
      name = blockInfo.getName();
      break;
    }
    auto &bi = biomes->getBiome(chunk->biomes[(x & 0xf) + (z & 0xf) * 16]);
    biome = bi.name;

    for (auto &item : getItems(x, y, z)) {
      entityIds[item->display()]++;
    }
  }

  QString entityStr;
  if (!entityIds.empty()) {
    QStringList entities;
    QMap<QString, int>::iterator it, itEnd = entityIds.end();
    for (it = entityIds.begin(); it != itEnd; ++it) {
      if (it.value() > 1) {
        entities << it.key() + ":" + QString::number(it.value());
      } else {
        entities << it.key();
      }
    }
    entityStr = entities.join(", ");
  }

  emit hoverTextChanged(tr("X:%1 Z:%2 (Nether: X:%8 Z:%9) - %3 - %4 (%5:%6) %7")
                        .arg(x)
                        .arg(z)
                        .arg(biome)
                        .arg(name)
                        .arg(block.id)
                        .arg(block.bd)
                        .arg(entityStr)
                        .arg(x/8)
                        .arg(z/8));
}

void MapView::addOverlayItem(QSharedPointer<OverlayItem> item) {
  overlayItems[item->type()].push_back(item);
}

void MapView::showOverlayItemTypes(const QSet<QString>& itemTypes) {
  overlayItemTypes = itemTypes;
}

int MapView::getY(int x, int z) {
  int cx = floor(x / 16.0);
  int cz = floor(z / 16.0);
  auto chunk = cache->fetch(cx, cz);
  return chunk ? chunk->depth[(x & 0xf) + (z & 0xf) * 16] : -1;
}

QList<QSharedPointer<OverlayItem>> MapView::getItems(int x, int y, int z) {
  QList<QSharedPointer<OverlayItem>> ret;
  int cx = floor(x / 16.0);
  int cz = floor(z / 16.0);
  auto chunk = cache->fetch(cx, cz);

  if (chunk) {
    double invzoom = 10.0 / zoom;
    for (auto &type : overlayItemTypes) {
      // generated structures
      for (auto &item : overlayItems[type]) {
        double ymin = 0;
        double ymax = depth;
        if (item->intersects(OverlayItem::Point(x, ymin, z),
                             OverlayItem::Point(x, ymax, z))) {
          ret.append(item);
        }
      }

      // entities
      auto itemRange = chunk->entities.equal_range(type);
      for (auto itItem = itemRange.first; itItem != itemRange.second;
          ++itItem) {
        double ymin = y - 4;
        double ymax = depth + 4;

        if ((*itItem)->intersects(
            OverlayItem::Point(x - invzoom/2, ymin, z - invzoom/2),
            OverlayItem::Point(x + 1 + invzoom/2, ymax, z + 1 + invzoom/2))) {
          ret.append(*itItem);
        }
      }
    }
  }
  return ret;
}

