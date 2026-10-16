# linked_list.py
# Teaches: a singly-linked list as a class, with insert/traverse/delete.
# Demonstrates head pointer, node chaining, and length tracking.
# Intermediate sample. Run: python3 linked_list.py


class Node:
    """One link in the chain: a value and a reference to the next node."""

    def __init__(self, value):
        self.value = value
        # `None` is Python's null. The last node's `next` is None,
        # which is how we recognize the end of the list.
        self.next = None


class LinkedList:
    """A singly-linked list with O(1) head insert and O(n) tail insert.

    We track `length` explicitly so `len()` is O(1); without it we'd
    have to walk the chain every time.
    """

    def __init__(self):
        self.head = None
        self.length = 0

    def __len__(self):
        # Implementing __len__ lets callers use the built-in len().
        return self.length

    def insert_head(self, value):
        """Prepend `value`. Constant-time: only the head changes."""
        new_node = Node(value)
        new_node.next = self.head  # old head becomes the second node
        self.head = new_node
        self.length += 1

    def insert_tail(self, value):
        """Append `value`. Linear-time: must walk to the last node."""
        new_node = Node(value)

        # Empty-list special case: the new node IS the head.
        if self.head is None:
            self.head = new_node
            self.length += 1
            return

        # Walk until we find the node whose `next` is None.
        cursor = self.head
        while cursor.next is not None:
            cursor = cursor.next
        cursor.next = new_node
        self.length += 1

    def delete_first(self, value):
        """Remove the first node holding `value`. Returns True on success."""
        # Empty list: nothing to delete.
        if self.head is None:
            return False

        # Special-case the head: there's no predecessor to rewire.
        if self.head.value == value:
            self.head = self.head.next
            self.length -= 1
            return True

        # General case: walk with a "previous" pointer so we can splice
        # the target out by setting prev.next = target.next.
        prev = self.head
        cursor = self.head.next
        while cursor is not None:
            if cursor.value == value:
                prev.next = cursor.next
                self.length -= 1
                return True
            prev = cursor
            cursor = cursor.next
        return False

    def to_list(self):
        """Return the values as a regular Python list (helpful for tests)."""
        out = []
        cursor = self.head
        while cursor is not None:
            out.append(cursor.value)
            cursor = cursor.next
        return out


def main():
    ll = LinkedList()

    # Build [10, 20, 30] by appending.
    for v in (10, 20, 30):
        ll.insert_tail(v)
    print("after appends:", ll.to_list(), "len=", len(ll))

    # Prepend 5 -> [5, 10, 20, 30].
    ll.insert_head(5)
    print("after head insert:", ll.to_list())

    # Delete a middle value.
    ok = ll.delete_first(20)
    print("delete 20 ok=", ok, "->", ll.to_list())

    # Delete the head.
    ok = ll.delete_first(5)
    print("delete 5  ok=", ok, "->", ll.to_list())

    # Try to delete something not present.
    ok = ll.delete_first(999)
    print("delete 999 ok=", ok, "->", ll.to_list())


if __name__ == "__main__":
    main()
