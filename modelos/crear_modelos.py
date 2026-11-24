import numpy as np
import tensorflow as tf

def crear(alpha, lado, nombre):
    base = tf.keras.applications.MobileNetV2(
        input_shape=(lado, lado, 3), alpha=alpha, include_top=False, weights=None)
    salida = tf.keras.layers.Conv2D(18, 1)(base.output)    # cabeza tipo detector (3 anclas x 6 valores)
    modelo = tf.keras.Model(base.input, salida)

    def datos():
        for _ in range(50):
            yield [np.random.rand(1, lado, lado, 3).astype(np.float32)]

    conv = tf.lite.TFLiteConverter.from_keras_model(modelo)
    conv.optimizations = [tf.lite.Optimize.DEFAULT]
    conv.representative_dataset = datos
    conv.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    conv.inference_input_type = tf.int8
    conv.inference_output_type = tf.int8
    tflite = conv.convert()
    open(nombre, "wb").write(tflite)

    interp = tf.lite.Interpreter(model_path=nombre)
    interp.allocate_tensors()
    entrada = interp.get_input_details()[0]
    salida_d = interp.get_output_details()[0]
    print(nombre, len(tflite), "bytes | entrada", entrada["shape"], entrada["dtype"].__name__,
          "| salida", salida_d["shape"])

crear(0.5, 224, "modelo_a.tflite")
crear(0.5, 160, "modelo_b.tflite")
