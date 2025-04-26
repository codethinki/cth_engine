module;
#include <cth/macro.hpp>
#include <cth/io/io_log.hpp>

export module cth.engine.render.dag;

import cth.io.log;
import cth.typ.ranges;

import std;


export namespace cth {
template<class Rng, class Edge>
concept dag_edge_range = cth::type::range_over_cpt<Rng, CPT(std::convertible_to<Edge>)>;

template<class Rng, class Node>
concept dag_node_range = cth::type::range_over<Rng, Node>;

/**
 * @brief directed acrylic graph
 * @tparam T type to use as node
 * @tparam U edge info type
 */
template<class T, class U>
struct dag {
private:
    using tag_t = struct {};
    cxpr static tag_t TAG{};

public:
    using node_t = T;
    using annotation_t = U;
    using edge_t = struct {
        node_t source;
        node_t target;
        annotation_t annotation;
    };
    using edge_init_list_t = std::initializer_list<edge_t>;

    dag() = default;

    template<dag_edge_range<edge_t> Rng>
    explicit dag(Rng const& edges, tag_t = TAG) : dag{} { insert(edges); }
    explicit dag(edge_init_list_t const& edges) : dag(edges, TAG) {}


    void insert(node_t node) { _connections[node]; }

    /**
     * @brief adds a node, its target and the edge
     */
    template<dag_edge_range<edge_t> Rng>
    void insert(Rng const& edges) { for(auto const& edge : edges) insert(edge); }

    void insert(edge_init_list_t const& edges) { insert<edge_init_list_t>(edges); }

    void insert(edge_t const& edge) { insert(edge.source, edge.target, edge.annotation); }

    void insert(node_t source, node_t target, annotation_t annotation) {
        auto& targets = getEdges(source);

        targets.insert(target);

        _annotations[{source, target}] = annotation;

        if(!_connections.contains(target)) _connections[target];
    }

    /**
     * @brief erases a whole node
     */
    void erase(node_t node) {
        if(!_connections.contains(node)) return;

        auto& targets = _connections.at(node);

        for(auto const& target : targets)
            _annotations.erase({node, target});

        _connections.erase(node);
    }

    /**
     * @brief removes an edge between source and target
     * @param source to remove from
     * @param target to remove
     */
    void erase(node_t source, node_t target) {
        verifyExists(source);

        if(auto& targets = _connections.at(source); targets.contains(target)) {
            targets.erase(target);
            _annotations.erase({source, target});
        }
    }

    /**
     * @brief erases all relations another node
     * @attention requires the id to be present
     */
    void erase(node_t source, dag_node_range<node_t> auto&& targets) {
        verifyExists(source);
        for(auto const& target : targets) erase(source, target);
    }

    /**
     * @brief erases dependencies from an id
     * @attention requires the id to be present
     */
    void erase(node_t node, dag_edge_range<edge_t> auto&& edges) {
        verifyExists(node);
        auto& existing = getEdges(node);
        for(auto dependency : edges) existing.erase(dependency);
    }

    /**
     * @brief finds the cyclic nodes
     */
    [[nodiscard]] std::vector<size_t> cyclics() const {
        auto connections = _connections;
        bool finished = false;


        while(!connections.empty() && !finished) {
            size_t oldSize = connections.size();

            std::erase_if(connections, [&connections](auto const& connection) {
                auto const& [node, nodeTargets] = connection;
                if(!nodeTargets.empty()) return false;


                for(auto& [_, targets] : connections)
                    std::erase_if(targets, [node](auto const& target) { return target == node; });
                return true;
            });

            finished = oldSize == connections.size();
        }
        return {std::from_range, connections | std::views::keys};
    }

    /**
     * @brief checks if the graph is cyclic DAG (no cyclic nodes)
     */
    [[nodiscard]] bool cyclic() const { return !cyclics().empty(); }

    [[nodiscard]] std::set<node_t> roots() const {
        std::set<node_t> roots{};
        for(auto const& [node, edges] : _connections)
            if(edges.empty()) roots.insert(node);
        return roots;
    }

    [[nodiscard]] std::set<node_t> destinations() const {
        std::set<node_t> destinations{std::from_range, _connections | std::views::keys};

        for(auto const& node : _connections | std::views::values | std::views::join)
            destinations.erase(node);
        return destinations;
    }

private:
    void verifyExists(node_t id) const {
        CTH_CRITICAL(!_connections.contains(id), "id: [{}] not present in dependency graph", id){}
    }

    auto& getEdges(node_t node) { return _connections[node]; }

    static node_t edge_target(edge_t edge) { return std::get<0>(edge); }

    std::map<node_t, std::set<node_t>> _connections{};
    std::map<std::pair<node_t, node_t>, annotation_t> _annotations{};

public:
    [[nodiscard]] size_t edgeCount() const {
        return std::ranges::fold_left(
            _connections | std::views::values,
            size_t{0},
            [](size_t const sum, auto const& set) { return sum + set.size(); }
        );
    }
    [[nodiscard]] bool contains(node_t node) const { return _connections.contains(node); }

    [[nodiscard]] auto& at(node_t node) const { return _connections.at(node); }
    [[nodiscard]] auto& map() const { return _connections; }

    [[nodiscard]] std::set<node_t> nodes() const { return {std::from_range, _connections | std::views::keys}; }
    [[nodiscard]] annotation_t annotation(node_t source, node_t target) const {
        CTH_CRITICAL(!_annotations.contains({source, target}), "the edge between source and target must exist"){}
        return _annotations.at({source, target});
    }

};
}
