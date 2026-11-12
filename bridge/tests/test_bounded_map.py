import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('bounded_map', Path(__file__).parents[1]/'bounded_map.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class MapTests(unittest.TestCase):
    def test_late_receipts_are_unknown_not_misattributed(self):
        values=m.BoundedMap(3)
        for number in range(20):values[number]='id-'+str(number)
        self.assertEqual(len(values), 3)
        self.assertIsNone(values.get(0))
        self.assertEqual(values.get(19), 'id-19')
    def test_update_does_not_grow_or_evict_newer_reference(self):
        values=m.BoundedMap(2);values[1]='a';values[2]='b';values[1]='edited';values[3]='c'
        self.assertEqual(dict(values), {1:'edited',3:'c'})
