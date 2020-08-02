import unittest
import numpy as np
from torch import nn

from util import prepare_trees
import tcnn

class TestTreeConvolution(unittest.TestCase):

    def test_example(self):
        # simple smoke test from the example file
        tree1 = (
            (0, 1),
            ((1, 2), ((0, 1),), ((-1, 0),)),
            ((-3, 0), ((2, 3),), ((1, 2),))
        )
        
        tree2 = (
            (16, 3),
            ((0, 1), ((5, 3),), ((2, 6),)),
            ((2, 9),)
        )

        trees = [tree1, tree2]
        
        # function to extract the left child of a node
        def left_child(x):
            assert isinstance(x, tuple)
            if len(x) == 1:
                # leaf.
                return None
            return x[1]

        # function to extract the right child of node
        def right_child(x):
            assert isinstance(x, tuple)
            if len(x) == 1:
                # leaf.
                return None
            return x[2]

        # function to transform a node into a (feature) vector,
        # should be a numpy array.
        def transformer(x):
            return np.array(x[0])


        prepared_trees = prepare_trees(trees, transformer, left_child, right_child)
        net = nn.Sequential(
            tcnn.BinaryTreeConv(2, 16),
            tcnn.TreeLayerNorm(),
            tcnn.TreeActivation(nn.ReLU()),
            tcnn.BinaryTreeConv(16, 8),
            tcnn.TreeLayerNorm(),
            tcnn.TreeActivation(nn.ReLU()),
            tcnn.BinaryTreeConv(8, 4),
            tcnn.TreeLayerNorm(),
            tcnn.TreeActivation(nn.ReLU()),
            tcnn.DynamicPooling()
        )

        # output: torch.Size([2, 4])
        shape = tuple(net(prepared_trees).shape)
        self.assertEqual(shape, (2, 4))

if __name__ == '__main__':
    unittest.main()
