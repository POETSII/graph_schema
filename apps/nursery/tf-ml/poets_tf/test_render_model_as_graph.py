import os
import tempfile

import tensorflow as tf
import numpy as np

import tensorflow.keras as keras
import tensorflow_datasets as tfds
import tensorflow_model_optimization as tfmot

import render_model_as_graph

script_path=os.path.dirname(os.path.realpath(__file__))

original_save_model="fmnist_conv5x5_pool2_d400_d10_original"
original_save_model_path=os.path.join(script_path, original_save_model)

pruned_save_model="fmnist_conv5x5_pool2_d400_d10_pruned"
pruned_save_model_path=os.path.join(script_path, pruned_save_model)

# Load MNIST dataset
mnist = tf.keras.datasets.fashion_mnist
(train_images, train_labels), (test_images, test_labels) = mnist.load_data()

# Normalize the input image so that each pixel value is between 0 to 1.
train_images = train_images / 255.0
test_images = test_images / 255.0

if os.path.exists(original_save_model_path):
    model = tf.keras.models.load_model(original_save_model_path)
else:
    model = keras.Sequential([
    keras.layers.InputLayer(input_shape=(28, 28)),
        keras.layers.Reshape(target_shape=(28, 28, 1)),
        keras.layers.Conv2D(filters=12, kernel_size=(5,5), activation='relu', name="c0"),
        keras.layers.MaxPooling2D(pool_size=(2, 2)),
        keras.layers.Flatten(),
    keras.layers.Dense(400, activation="relu",name="l1"),
    keras.layers.Dense(10,activation='softmax',name="l0")
    ])

    # Train the digit classification model
    model.compile(optimizer='adam',
                loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
                metrics=['accuracy'])

    model.fit(
        train_images,
        train_labels,
        epochs=8,
        validation_split=0.1,
    )

    model.save(original_save_model_path)


if os.path.exists(pruned_save_model_path):
    model_for_pruning = tf.keras.models.load_model(pruned_save_model_path)
else:

    ####################################################################
    ## Sparsification

    prune_low_magnitude = tfmot.sparsity.keras.prune_low_magnitude

    # Compute end step to finish pruning after 2 epochs.
    batch_size = 128
    epochs = 4
    validation_split = 0.1 # 10% of training set will be used for validation set. 

    num_images = train_images.shape[0] * (1 - validation_split)
    end_step = np.ceil(num_images / batch_size).astype(np.int32) * epochs

    # Define model for pruning.
    pruning_params_c0 = {
        'pruning_schedule': tfmot.sparsity.keras.PolynomialDecay(initial_sparsity=0.50,
                                                                final_sparsity=0.9,
                                                                begin_step=0,
                                                                end_step=end_step)
    }
    pruning_params_l2 = {
        'pruning_schedule': tfmot.sparsity.keras.PolynomialDecay(initial_sparsity=0.50,
                                                                final_sparsity=0.999,
                                                                begin_step=0,
                                                                end_step=end_step)
    }
    pruning_params_l1 = {
        'pruning_schedule': tfmot.sparsity.keras.PolynomialDecay(initial_sparsity=0.50,
                                                                final_sparsity=0.999,
                                                                begin_step=0,
                                                                end_step=end_step)
    }
    pruning_params_l0 = {
        'pruning_schedule': tfmot.sparsity.keras.PolynomialDecay(initial_sparsity=0.50,
                                                                final_sparsity=0.99,
                                                                begin_step=0,
                                                                end_step=end_step)
    }

    def apply_pruning_to_selected(layer):
        if layer.name=="c0":
            return tfmot.sparsity.keras.prune_low_magnitude(layer, **pruning_params_c0)
        if layer.name=="l1":
            return tfmot.sparsity.keras.prune_low_magnitude(layer, **pruning_params_l1)
        if layer.name=="l2":
            return tfmot.sparsity.keras.prune_low_magnitude(layer, **pruning_params_l2)
        if layer.name=="l0":
            return tfmot.sparsity.keras.prune_low_magnitude(layer, **pruning_params_l0)
        return layer

    model_for_pruning = tf.keras.models.clone_model(
        model,
        clone_function=apply_pruning_to_selected,
    )

    #model_for_pruning = prune_low_magnitude(model, **pruning_params)

    # `prune_low_magnitude` requires a recompile.
    model_for_pruning.compile(optimizer='adam',
                loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
                metrics=['accuracy'])

    model_for_pruning.summary()

    logdir = tempfile.mkdtemp()

    es = tf.keras.callbacks.EarlyStopping(monitor='val_accuracy', mode='max', min_delta=2, restore_best_weights=True)

    callbacks = [
        tfmot.sparsity.keras.UpdatePruningStep(),
        tfmot.sparsity.keras.PruningSummaries(log_dir=logdir),
        es
    ]

    	

    
    model_for_pruning.fit(train_images, train_labels,
                    batch_size=batch_size, epochs=epochs, validation_split=validation_split,
                    callbacks=callbacks)

    model.save(pruned_save_model_path)

render_model_as_graph.render_model_as_graph(pruned_save_model, model_for_pruning.layers, (28,28))