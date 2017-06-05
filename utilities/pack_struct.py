import struct
import binascii


class Cvar:

    def set_struct_char(self):
        if self.var_type is "uint8_t" or self.var_type is "char":
            self.struct_char = 'c'
        elif self.var_type is "bool":
            self.struct_char = 'b'
        elif self.var_type is "float":
            self.struct_char = 'f'
        else:
            raise ValueError('Undefined var_type')

    def __init__(self, _var_name, _var_type, _var_length):
        self.var_name = _var_name
        self.var_type = _var_type
        self.var_length = _var_length
        self.set_struct_char()

    def insert_value(self, value):
        self.value = value

class Cstruct:
    def __init__(self):
        self.variables = {}

    def add_variable(self, var_name, var_type, var_length):
        self.variables[var_name] = (Cvar(var_name, var_type, var_length))

    def create_struct_string(self):
        self.struct_string = ''

        for key, var in self.variables:
            for i in range(var.var_length):
                self.struct_string += var.struct_char

    def add_variables(self, var_list):
        for val in var_list:
            self.add_variable(val['var_name'], val['var_type'], val['var_length'])


if __name__ == '__main__':
    sl_config = [{'var_name' : 'key',
                  'var_type' : 'uint8_t',
                  'var_length' : 16},
                 {'var_name' : 'sl_id',
                  'var_type' : 'uint8_t',
                  'var_length' : 4},
                 {'var_name' : 'freq',
                  'var_type' : 'float',
                  'var_length' : 1},
                 {'var_name' : 'mod_conf',
                  'var_type' : 'uint8_t',
                  'var_length' :1},
                 {'var_name' : 'node_address',
                  'var_type' : 'uint8_t',
                  'var_length' : 1}]

    sl_config_struct = Cstruct()

    sl_config.struct.variables['key'].insert_value('hello')
    sl_config.struct.variables['sl_id'].insert_value(99)
    sl_config.struct.variables['freq'].insert_value(915.0)
    sl_config.struct.variables['mod_conf'].insert_value(0)
    sl_config.struct.variables['node_address'].insert_value(4)


    sl_config_struct.add_variables(sl_config)

    sl_config_struct.create_struct_string()




