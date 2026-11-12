"""Bound volatile message correlation without recording contents or identities."""
from collections import OrderedDict

class BoundedMap(OrderedDict):
    def __init__(self, limit=8192):
        super().__init__()
        self.limit = limit
    def __setitem__(self, key, value):
        super().__setitem__(key, value)
        self.move_to_end(key)
        while len(self) > self.limit:
            self.popitem(last=False)
