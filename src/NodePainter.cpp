#include "NodePainter.hpp"

#include <cmath>

#include <QtCore/QMargins>

#include "StyleCollection.hpp"
#include "PortType.hpp"
#include "NodeGraphicsObject.hpp"
#include "NodeGeometry.hpp"
#include "NodeState.hpp"
#include "NodeIndex.hpp"
#include "FlowSceneModel.hpp"
#include "NodePainterDelegate.hpp"
#include "FlowScene.hpp"

namespace QtNodes {

void
NodePainter::
paint(QPainter* painter,
      NodeGraphicsObject const & graphicsObject)
{
  NodeGeometry const& geom = graphicsObject.geometry();

  NodeState const& state = graphicsObject.nodeState();

  geom.recalculateSize(painter->font());

  //--------------------------------------------

  drawNodeRect(painter, graphicsObject);

  drawConnectionPoints(painter, graphicsObject);

  drawFilledConnectionPoints(painter, graphicsObject);

  drawModelName(painter, graphicsObject);

  drawEntryLabels(painter, graphicsObject);

  drawResizeRect(painter, graphicsObject);

  drawValidationRect(painter, graphicsObject);

  /// call custom painter
  if (auto painterDelegate = graphicsObject.index().model()->nodePainterDelegate(graphicsObject.index()))
  {
    painterDelegate->paint(painter, graphicsObject);
  }
}


void
NodePainter::
drawNodeRect(QPainter* painter, NodeGraphicsObject const & graphicsObject)
{
  FlowSceneModel& model = *graphicsObject.flowScene().model();
  
  NodeStyle const& nodeStyle = model.nodeStyle(graphicsObject.index());
  NodeGeometry const& nodeGeometry = graphicsObject.geometry();

  auto color = graphicsObject.isSelected()
               ? nodeStyle.SelectedBoundaryColor
               : nodeStyle.NormalBoundaryColor;

  if (nodeGeometry.hovered())
  {
    QPen p(color, nodeStyle.HoveredPenWidth);
    painter->setPen(p);
  }
  else
  {
    QPen p(color, nodeStyle.PenWidth);
    painter->setPen(p);
  }

  QLinearGradient gradient(QPointF(0.0, 0.0),
                           QPointF(2.0, nodeGeometry.height()));

  gradient.setColorAt(0.0, nodeStyle.GradientColor0);
  gradient.setColorAt(0.03, nodeStyle.GradientColor1);
  gradient.setColorAt(0.97, nodeStyle.GradientColor2);
  gradient.setColorAt(1.0, nodeStyle.GradientColor3);

  painter->setBrush(gradient);

  float diam = nodeStyle.ConnectionPointDiameter;

  QRectF boundary( -diam, -diam, 2.0 * diam + nodeGeometry.width(), 2.0 * diam + nodeGeometry.height());

  double const radius = 3.0;

  painter->drawRoundedRect(boundary, radius, radius);
}


void
NodePainter::
drawConnectionPoints(QPainter* painter, NodeGraphicsObject const & graphicsObject)
{
  auto const &connectionStyle      = StyleCollection::connectionStyle();
  NodeState const& nodeState       = graphicsObject.nodeState();
  NodeGeometry const& nodeGeometry = graphicsObject.geometry();
  const FlowSceneModel& model      = *graphicsObject.flowScene().model();
  NodeStyle const& nodeStyle       = model.nodeStyle(graphicsObject.index());

  float diameter = nodeStyle.ConnectionPointDiameter;
  auto  reducedDiameter = diameter * 0.6;

  auto drawPoints =
    [&](PortType portType)
    {
      size_t n = nodeState.getEntries(portType).size();

      for (size_t i = 0; i < n; ++i)
      {

        QPointF p = nodeGeometry.portScenePosition(i, portType);

        auto const & dataType = model.nodePortDataType(graphicsObject.index(), portType, i);

        bool canConnect = (nodeState.getEntries(portType)[i].empty() ||
                            model.nodePortConnectionPolicy(graphicsObject.index(), portType, i) == ConnectionPolicy::Many);

        double r = 1.0;
        if (nodeState.isReacting() &&
            canConnect &&
            portType == nodeState.reactingPortType())
        {

          auto   diff = nodeGeometry.draggingPos() - p;
          double dist = std::sqrt(QPointF::dotProduct(diff, diff));
          bool   typeConvertable = false;

          {
            if (portType == PortType::In)
            {
              typeConvertable = model.converterNode(nodeState.reactingDataType(), dataType) != "";
            }
            else
            {
              typeConvertable = model.converterNode(dataType, nodeState.reactingDataType()) != "";
            }
          }

          if (nodeState.reactingDataType().id == dataType.id || typeConvertable)
          {
            double const thres = 40.0;
            r = (dist < thres) ?
                (2.0 - dist / thres ) :
                1.0;
          }
          else
          {
            double const thres = 80.0;
            r = (dist < thres) ?
                (dist / thres) :
                1.0;
          }
        }

        if (connectionStyle.useDataDefinedColors())
        {
          painter->setBrush(connectionStyle.normalColor(dataType.id));
        }
        else
        {
          painter->setBrush(nodeStyle.ConnectionPointColor);
        }

        painter->drawEllipse(p,
                             reducedDiameter * r,
                             reducedDiameter * r);
      }
    };

  drawPoints(PortType::Out);
  drawPoints(PortType::In);
}


void
NodePainter::
drawFilledConnectionPoints(QPainter * painter, NodeGraphicsObject const & graphicsObject)
{
  auto const& connectionStyle = StyleCollection::connectionStyle();
  NodeState const& state      = graphicsObject.nodeState();
  NodeGeometry const& geom    = graphicsObject.geometry();
  FlowSceneModel const& model = *graphicsObject.index().model();
  NodeStyle const& nodeStyle  = model.nodeStyle(graphicsObject.index());

  auto diameter = nodeStyle.ConnectionPointDiameter;

  auto drawPoints =
    [&](PortType portType)
    {
      size_t n = state.getEntries(portType).size();

      for (size_t i = 0; i < n; ++i)
      {
        QPointF p = geom.portScenePosition(i, portType);

        if (!state.getEntries(portType)[i].empty())
        {
          auto const & dataType = model.nodePortDataType(graphicsObject.index(), portType, i);

          if (connectionStyle.useDataDefinedColors())
          {
            QColor const c = connectionStyle.normalColor(dataType.id);
            painter->setPen(c);
            painter->setBrush(c);
          }
          else
          {
            painter->setPen(nodeStyle.FilledConnectionPointColor);
            painter->setBrush(nodeStyle.FilledConnectionPointColor);
          }

          painter->drawEllipse(p,
                               diameter * 0.4,
                               diameter * 0.4);
        }
      }
    };

  drawPoints(PortType::Out);
  drawPoints(PortType::In);
}


void
NodePainter::
drawModelName(QPainter * painter, NodeGraphicsObject const & graphicsObject)
{
  NodeStyle const& nodeStyle = graphicsObject.index().model()->nodeStyle(graphicsObject.index());
  FlowSceneModel const& model = *graphicsObject.index().model();
  NodeGeometry const& geom = graphicsObject.geometry();

  QString const &name = model.nodeCaption(graphicsObject.index());
  
  if (name.isEmpty()) {
    return;
  }

  QFont f = painter->font();

  f.setBold(true);

  QFontMetrics metrics(f);

  auto rect = metrics.boundingRect(name);

  QPointF position((geom.width() - rect.width()) / 2.0,
                   (geom.spacing() + geom.entryHeight()) / 3.0);

  painter->setFont(f);
  painter->setPen(nodeStyle.FontColor);
  painter->drawText(position, name);

  f.setBold(false);
  painter->setFont(f);
}


void
NodePainter::
drawEntryLabels(QPainter * painter, NodeGraphicsObject const & graphicsObject)
{
  NodeState const& state = graphicsObject.nodeState();
  NodeGeometry const& geom = graphicsObject.geometry();
  FlowSceneModel const& model = *graphicsObject.index().model();
  QFontMetrics const & metrics =
    painter->fontMetrics();

  auto drawPoints =
    [&](PortType portType)
    {
      auto const &nodeStyle = graphicsObject.index().model()->nodeStyle(graphicsObject.index());

      auto& entries = state.getEntries(portType);

      size_t n = entries.size();

      for (size_t i = 0; i < n; ++i)
      {
        QPointF p = geom.portScenePosition(i, portType);

        if (entries[i].empty())
          painter->setPen(nodeStyle.FontColorFaded);
        else
          painter->setPen(nodeStyle.FontColor);

        QString s = model.nodePortCaption(graphicsObject.index(), portType, i);

        if (s.isEmpty())
        {
          s = model.nodePortDataType(graphicsObject.index(), portType, i).name;
        }

        auto rect = metrics.boundingRect(s);

        p.setY(p.y() + rect.height() / 4.0);

        switch (portType)
        {
          case PortType::In:
            p.setX(5.0);
            break;

          case PortType::Out:
            p.setX(geom.width() - 5.0 - rect.width());
            break;

          default:
            break;
        }

        painter->drawText(p, s);
      }
    };

  drawPoints(PortType::Out);

  drawPoints(PortType::In);
}


void
NodePainter::
drawResizeRect(QPainter * painter,
                     NodeGraphicsObject const & graphicsObject)
{
  FlowSceneModel const& model = *graphicsObject.index().model();

  if (model.nodeResizable(graphicsObject.index()))
  {
    painter->setBrush(Qt::gray);

    painter->drawEllipse(graphicsObject.geometry().resizeRect());
  }
}


void
NodePainter::
drawValidationRect(QPainter * painter, NodeGraphicsObject const & graphicsObject)
{
  FlowSceneModel const& model = *graphicsObject.index().model();
  NodeGeometry const& geom = graphicsObject.geometry();

  auto modelValidationState = model.nodeValidationState(graphicsObject.index());

  if (modelValidationState != NodeValidationState::Valid)
  {
    NodeStyle const& nodeStyle = model.nodeStyle(graphicsObject.index());

    auto color = graphicsObject.isSelected()
                 ? nodeStyle.SelectedBoundaryColor
                 : nodeStyle.NormalBoundaryColor;

    if (geom.hovered())
    {
      QPen p(color, nodeStyle.HoveredPenWidth);
      painter->setPen(p);
    }
    else
    {
      QPen p(color, nodeStyle.PenWidth);
      painter->setPen(p);
    }

    //Drawing the validation message background
    if (modelValidationState == NodeValidationState::Error)
    {
      painter->setBrush(nodeStyle.ErrorColor);
    }
    else
    {
      painter->setBrush(nodeStyle.WarningColor);
    }

    double const radius = 3.0;

    float diam = nodeStyle.ConnectionPointDiameter;

    QRectF boundary(-diam,
                    -diam + geom.height() - geom.validationHeight(),
                    2.0 * diam + geom.width(),
                    2.0 * diam + geom.validationHeight());

    painter->drawRoundedRect(boundary, radius, radius);

    painter->setBrush(Qt::gray);

    //Drawing the validation message itself
    QString const &errorMsg = model.nodeValidationMessage(graphicsObject.index());

    QFont f = painter->font();

    QFontMetrics metrics(f);

    auto rect = metrics.boundingRect(errorMsg);

    QPointF position((geom.width() - rect.width()) / 2.0,
                     geom.height() - (geom.validationHeight() - diam) / 2.0);

    painter->setFont(f);
    painter->setPen(nodeStyle.FontColor);
    painter->drawText(position, errorMsg);
  }
}

} // namespace QtNodes
