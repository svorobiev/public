#include <unordered_map>

using user_key_type = std::string;
using user_value_type = std::string;

struct intrusive_list_links {
    intrusive_list_links *flink = nullptr;
    intrusive_list_links *blink = nullptr;
};

// this is *the* classic way to implement a doubly linked list.
// Notice that neither insert nor remove have any branching. Remove also doesn't
// know where HEAD is! 
void insert_tail_list(intrusive_list_links *head, intrusive_list_links *entry)
{
    entry->flink = head;
    intrusive_list_links *last = head->blink;
    entry->blink = last;

    last->flink = entry;
    head->blink = entry;
}

void remove_from_list(const intrusive_list_links *entry)
{
    intrusive_list_links *prev = entry->blink;
    intrusive_list_links *next = entry->flink;

    prev->flink = next;
    next->blink = prev;
}

struct map_value_type {
    intrusive_list_links links; // this chains the value with the other list entries
    user_value_type m_value;

    // throw in implicit conversions
    operator user_value_type&() { return m_value; }
    operator user_value_type() const { return m_value; }
};

class mymap {
    using map_type = std::unordered_map<user_key_type, map_value_type>;

    // I want to do pointer tricks: 
    static_assert(std::is_standard_layout<map_type::value_type>::value);
    static map_type::value_type *to_value_type(intrusive_list_links *links)
    {
        constexpr size_t offset_of_links_in_value = offsetof(map_value_type, links);
        constexpr size_t offset_of_value_in_map_value_type = offsetof(map_type::value_type, second);
        // yay, pointer tricks:
        return (map_type::value_type *)
            (
                ((char*)links)
                - offset_of_links_in_value
                - offset_of_value_in_map_value_type);
        // this allows me to take intrusive_list_links*, and cast it to the STL's pair<K, V>
        // that has the key and value for the map element 
    }
    
    // this is the actual map
    map_type m_data;

    // stable traversal order
    intrusive_list_links m_head;

  public:
    const map_type& data() const
    {
        // all read-only accessors can use unordered_map interface
        return m_data;
    }

    // we also need something that would allow to mutate the values in the map

    mymap()
    {
        // this is how an empty list looks like:
        m_head.flink = &m_head;
        m_head.blink = &m_head;
    }

    struct iterator {
        intrusive_list_links* position;
        iterator& operator++() { position = position->flink; return *this;};
        map_type::value_type& operator*() { return *to_value_type(position); }
        map_type::value_type* operator->() {return to_value_type(position);}
        bool operator != (const iterator &other) const  {return position != other.position;}
        
    };

    iterator begin() { return {m_head.flink};}
    iterator end() {return {&m_head};}

    // traverse the map in the insertion order like this:
    // mymap foo;
    // ...
    // for (auto it : foo) { whatever; }


    // inserts an element if an equivalent element does not exist, else does nothing.
    // returns a reference to the inserted or previously existing KV
    map_type::value_type *insert(const user_key_type &k, const user_value_type &v)
    {
        auto &value_already_in_map = m_data[k];
        // Is this a new value? default-constructed hash table node will have nulls
        // for flink and blink:
        if (value_already_in_map.links.flink == nullptr) {
            // yes, it is new
            insert_tail_list(&m_head, &value_already_in_map.links);
        }
        return to_value_type(&value_already_in_map.links);   
    }

    // removes an element using an iterator into the actual map
    void remove(map_type::const_iterator it)
    {
        remove_from_list(&it->second.links);
        m_data.erase(it);
    }
};

#include <iostream>

int main()
{
    mymap foo;
    auto entry = foo.insert(std::string("hello, take one"), std::string("world"));
    std::string &value = entry->second;
    value = "WORLD";
    foo.insert(std::string("hello, take two"), std::string("world"));
    foo.insert(std::string("hello, take three"), std::string("world"));
    for (auto kv : foo) {
        std::cout << kv.first << " " << kv.second.m_value << '\n';
    }
}
