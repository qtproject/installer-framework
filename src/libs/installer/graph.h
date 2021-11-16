/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef GRAPH_H
#define GRAPH_H

#include <QHash>
#include <QList>
#include <QPair>
#include <QSet>

namespace QInstaller {

template <class T> class Graph
{
public:
    inline Graph() {}
    explicit Graph(const QList<T> &nodes)
    {
        addNodes(nodes);
    }

    const QList<T> nodes() const
    {
        return m_graph.keys();
    }

    void addNode(const T &node)
    {
        m_graph.insert(node, QSet<T>());
    }

    void addNodes(const QList<T> &nodes)
    {
        foreach (const T &node, nodes)
            addNode(node);
    }

    QList<T> edges(const T &node) const
    {
        return m_graph.value(node).values();
    }

    void addEdge(const T &node, const T &edge)
    {
        m_graph[node].insert(edge);
    }

    void addEdges(const T &node, const QList<T> &edges)
    {
        foreach (const T &edge, edges)
            addEdge(node, edge);
    }

    bool hasCycle() const
    {
        return m_hasCycle;
    }

    QPair<T, T> cycle() const
    {
        return m_cycle;
    }

    QList<T> sort() const
    {
        QSet<T> visitedNodes;
        QList<T> resolvedNodes;

        m_hasCycle = false;
        m_cycle = qMakePair(T(), T());
        foreach (const T &node, nodes())
            visit(node, &resolvedNodes, &visitedNodes);
        return resolvedNodes;
    }

    QList<T> sortReverse() const
    {
        QList<T> result = sort();
        std::reverse(result.begin(), result.end());
        return result;
    }

private:
    void visit(const T &node, QList<T> *const resolvedNodes, QSet<T> *const visitedNodes) const
    {
        if (m_hasCycle)
            return;

        // if we visited this node already
        if (visitedNodes->contains(node)) {
            // and if the node is already in the ordered list
            if (resolvedNodes->contains(node))
                return;

            m_hasCycle = true;
            m_cycle.second = node;
            return; // if not yet in the ordered list, we detected a cycle
        }

        // mark this node as visited
        visitedNodes->insert(node);

        m_cycle.first = node;
        // recursively visit all adjacency
        foreach (const T &adjacency, edges(node))
            visit(adjacency, resolvedNodes, visitedNodes);

        // append this node to the ordered list
        resolvedNodes->append(node);
    }

private:
    mutable bool m_hasCycle;
    QHash<T, QSet<T> > m_graph;
    mutable QPair<T,T> m_cycle;
};

}
#endif // GRAPH_H
