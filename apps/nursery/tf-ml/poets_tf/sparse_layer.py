import tensorflow as tf
import numpy as np

import tensorflow.keras as keras
import tensorflow_datasets as tfds
import tensorflow_model_optimization as tfmot

class SparseLayer(keras.layers.Layer, tfmot.sparsity.keras.PrunableLayer):
    def __init__(self, size, sparsity=0.5, name="sparse", **kwargs):
        super(SparseLayer, self).__init__(**kwargs)
        self.sparsity=sparsity
        self.size=size
        self._name=name

    def build(self, input_shape):
        def mask_init(shape,dtype=None):
            print(dtype)
            dtype=dtype or np.float
            mask=np.random.rand(*shape)
            mask=np.less(mask , self.sparsity, dtype=np.float)
            return mask
        
        self.mask=self.add_weight(name=f"{self._name}_m",
            shape=(input_shape[-1],self.size),
            initializer=mask_init, trainable=False
        )
        self.w = self.add_weight(name=f"{self._name}_w",
            shape=(input_shape[-1], self.size),
            initializer="random_normal",
            trainable=True,
        )
        self.b = self.add_weight(name=f"{self._name}_b",
            shape=(self.size,), initializer="random_normal", trainable=True
        )

    def call(self, inputs):
        mm=tf.math.multiply(self.w, self.mask)
        return tf.nn.relu( tf.matmul(inputs, self.w) + self.b )
    
    def get_config(self):
        config = super(SparseLayer, self).get_config()
        config.update({"size": self.size})
        config.update({"sparsity": self.sparsity})
        config.update({"name": self._name})
        return config
    
    def get_prunable_weights(self):
        return [self.w]