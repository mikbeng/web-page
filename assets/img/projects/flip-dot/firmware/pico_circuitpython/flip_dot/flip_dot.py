import time
import random

try:
    import microcontroller
    import board
    import digitalio
    #import adafruit_datetime
    import supervisor
except ImportError:
    print("Import error. Using dummy class")
    # Dummy class for pico digital io
    class digitalio:
        class Direction:
            """Defines the direction of a digital pin"""

            def __init__(self) -> None:
                """Enum-like class to define which direction the digital values are
                going."""
                ...
            INPUT= 0
            """Read digital data in"""

            OUTPUT= 1
            """Write digital data out"""
        class DigitalInOut:
            direction = 0
            value = 0

            def __init__(self, gpio):
                pass

    class board:
        GP0= 1
        GP1= 1
        GP2= 1
        GP3= 1
        GP4= 1
        GP5= 1
        GP6= 1
        GP7= 1
        GP8= 1
        GP9= 1
        GP10= 1
        GP11= 1
        GP12= 1
        GP13= 1
        GP14= 1
        GP15= 1
        GP16= 1
        GP17= 1
        GP18= 1
        GP19= 1
        GP20= 1
        GP21= 1
        GP22= 1
        LED=1


DEBUG_TIME = False
########## Debug Pin #############
#debug = digitalio.DigitalInOut(board.GP14)  #bottom left pin on pico
#debug.direction = digitalio.Direction.OUTPUT
#debug.value = 0
pin_value = 1

class Delay_Ticks:
    def __init__(self):
        self._TICKS_PERIOD = 1<<29
        self._TICKS_MAX = (self._TICKS_PERIOD-1)
        self._TICKS_HALFPERIOD = (self._TICKS_PERIOD//2)

    def ticks_diff(self, ticks1, ticks2):
        #"Compute the signed difference between two ticks values, assuming that they are within 2**28 ticks"
        diff = (ticks1 - ticks2) & self._TICKS_MAX
        diff = ((diff + self._TICKS_HALFPERIOD) & self._TICKS_MAX) - self._TICKS_HALFPERIOD
        return diff

    def ticks_less(self, ticks1, ticks2):
        #"Return true iff ticks1 is less than ticks2, assuming that they are within 2**28 ticks"
        return self.ticks_diff(ticks1, ticks2) < 0

    def delay_ms(self, delay_ms):
        ticks1 = supervisor.ticks_ms()
        #ticks_wait = delay_ms*
        while(self.ticks_diff(supervisor.ticks_ms(), ticks1) < delay_ms):
            pass

delay = Delay_Ticks()

def shuffled(array: list, seed: int = None) -> list:
    if seed is None:
        seed = random.randint(1, 0xFFFE)

    length_ = len(array)
    shuffled = [0]*length_
    seed_pos = seed ^ 0xFFFF

    for i in list(reversed(range(length_))):
        index = seed_pos % (i + 1)
        shuffled[i] = array.pop(index)

    return shuffled

def inv_value(value,inv):
    if inv:
        value = not value
    return value

def Decimal_to_Bin(number, bits):
    #Add limit checks on number? (Based on bits)
    binary_arr = []
    for i in range(bits-1,-1,-1):
        mask = 1 << i
        bit = 1 if (mask & number) else 0
        binary_arr.append(bit)

    return binary_arr

class demux_74HC4514:
    def __init__(self, pin_map):
        self.enc_dict = {
            0: 1<<0,
            1: 1<<1,
            2: 1<<2,
            3: 1<<3,
            4: 1<<4,
            5: 1<<5,
            6: 1<<6,
            7: 1<<7,
            8: 1<<8,
            9: 1<<9,
            10: 1<<10,
            11: 1<<11,
            12: 1<<12,
            13: 1<<13,
            14: 1<<14,
            15: 1<<15,
        }

        self.enc_dict_faster = {
            1<<0: [0,0,0,0],
            1<<1: [1,0,0,0],
            1<<2: [0,1,0,0],
            1<<3: [1,1,0,0],
            1<<4: [0,0,1,0],
            1<<5: [1,0,1,0],
            1<<6: [0,1,1,0],
            1<<7: [1,1,1,0],
            1<<8: [0,0,0,1],
            1<<9: [1,0,0,1],
            1<<10: [0,1,0,1],
            1<<11: [1,1,0,1],
            1<<12: [0,0,1,1],
            1<<13: [1,0,1,1],
            1<<14: [0,1,1,1],
            1<<15: [1,1,1,1],
        }

        # list out keys and values separately
        self.A_list = list(self.enc_dict.keys())
        self.Y_list = list(self.enc_dict.values())

        self.pin_A0 = pin_map['input']['A0'][0]
        self.pin_A1 = pin_map['input']['A1'][0]
        self.pin_A2 = pin_map['input']['A2'][0]
        #Check if A3 exist (i.e. if we have col or row pinmap)
        if(pin_map.get('input', {}).get('A3', {}) != {}):
            self.pin_A3 = pin_map['input']['A3'][0]
            self.pin_A3.direction = digitalio.Direction.OUTPUT
        else:
            self.pin_A3 = None

        self.pin_A0.direction = digitalio.Direction.OUTPUT
        self.pin_A1.direction = digitalio.Direction.OUTPUT
        self.pin_A2.direction = digitalio.Direction.OUTPUT

    def encode(self, input):
        #'Out to in'
        position = self.Y_list.index(input)
        return self.A_list[position]

    def decode(self, input):
        #'in to out'
        #output = 0b1111
        #output = output & (~(1 << input) & 0xF)
        #return output
        return(self.enc_dict[input])

    #@micropython.native
    def set_output(self, output_pos):
        #Set input pins so that E_row 'row' is enabled!

        ##slow
        # input = self.encode(output_pos) #Input corresponds to the states of A0 and A1 as: 0bA1A0
        # level_A0 = bool((input>>0) & 0b1)
        # level_A1 = bool((input>>1) & 0b1)
        # level_A2 = bool((input>>2) & 0b1)
        # level_A3 = bool((input>>3) & 0b1)

        #enable gpios
        # print("74HC4514 input:")
        # print('pin_A0={}'.format(level_A0))
        # print('pin_A1={}'.format(level_A1))
        # print('pin_A2={}'.format(level_A2))
        # print('pin_A3={}'.format(level_A3))

        # pin_A0_value = self.enc_dict_faster[output_pos][0]
        # pin_A1_value = self.enc_dict_faster[output_pos][1]
        # pin_A2_value = self.enc_dict_faster[output_pos][2]
        # pin_A3_value = not (self.enc_dict_faster[output_pos][3])

        ##Fastest!
        binary = Decimal_to_Bin(output_pos,4)
        self.pin_A0.value = binary[3]
        self.pin_A1.value = binary[2]
        self.pin_A2.value = binary[1]
        self.pin_A3.value = not (binary[0])     #On column: Connect to 2A/2B header pin 18. INVERTED (Not used for row)

class Demux_74HC139:
    def __init__(self, pin_map):

        self.enc_dict = {
            0b00: 0b1110,
            0b01: 0b1101,
            0b10: 0b1011,
            0b11: 0b0111}

        self.enc_dict_fast = {
            0: [0,0],
            1: [0,1],
            2: [1,0],
            3: [1,1],}

        # list out keys and values separately
        self.A_list = list(self.enc_dict.keys())
        self.Y_list = list(self.enc_dict.values())

        self.pin_1A0 = pin_map['input']['1A0'][0]
        self.pin_1A1 = pin_map['input']['1A1'][0]
        self.pin_2A0 = pin_map['input']['2A0'][0]
        self.pin_2A1 = pin_map['input']['2A1'][0]

        self.pin_1E = pin_map['input']['1E'][0]
        self.pin_2E = pin_map['input']['2E'][0]

        self.pin_1A0_inv = pin_map['input']['1A0'][1]
        self.pin_1A1_inv = pin_map['input']['1A1'][1]
        self.pin_2A0_inv = pin_map['input']['2A0'][1]
        self.pin_2A1_inv = pin_map['input']['2A1'][1]

        self.pin_1E_inv = pin_map['input']['1E'][1]
        self.pin_2E_inv = pin_map['input']['2E'][1]

        self.pin_1A0.direction = digitalio.Direction.OUTPUT
        self.pin_1A1.direction = digitalio.Direction.OUTPUT
        self.pin_2A0.direction = digitalio.Direction.OUTPUT
        self.pin_2A1.direction = digitalio.Direction.OUTPUT

        self.pin_1E.direction = digitalio.Direction.OUTPUT
        self.pin_2E.direction = digitalio.Direction.OUTPUT

    def __enable_rowgrp(self):
        #Enable the outputs (set E pin to low)
        self.pin_1E.value = True

    def __enable_colgrp(self):
        #Enable the outputs (set E pin to low)
        self.pin_2E.value = True

    def __disable_rowgrp(self):
        #Enable the outputs (set E pin to low)
        self.pin_1E.value = False

    def __disable_colgrp(self):
        #Enable the outputs (set E pin to low)
        self.pin_2E.value = False

    def encode(self, input):
        #'Out to in'
        position = self.Y_list.index(input)
        return self.A_list[position]

    def decode(self, input):
        #'in to out'
        #output = 0b1111
        #output = output & (~(1 << input) & 0xF)
        #return output
        return(self.enc_dict[input])

    #@micropython.native
    def set_row_output(self, row_grp, output_pos, row_demux):
        ## SLower
        # output = ~(0b0001<<output_pos) & 0xF
        # input = self.encode(output) #Input corresponds to the states of A0 and A1 as: 0bA1A0
        # level_A0 = bool((input>>0) & 0b1)
        # level_A1 = bool((input>>1) & 0b1)

        ##Faster
        # self.pin_1A0.value = self.enc_dict_fast[output_pos][1]
        # self.pin_1A1.value = self.enc_dict_fast[output_pos][0]

        ##Fastest!
        binary = Decimal_to_Bin(row_grp,2)
        self.pin_1A0.value = binary[1]
        self.pin_1A1.value = binary[0]

        ##Fastest!
        binary = Decimal_to_Bin(output_pos,4)
        row_demux.pin_A0.value = binary[3]
        row_demux.pin_A1.value = binary[2]
        row_demux.pin_A2.value = binary[1]
        #self.pin_A3.value = not (binary[0])     #On column: Connect to 2A/2B header pin 18. INVERTED (Not used for row)


    #@micropython.native
    def set_col_output(self, col_grp, output_pos, col_demux):
        ## SLower
        # output = ~(0b0001<<output_pos) & 0xF
        # input = self.encode(output) #Input corresponds to the states of A0 and A1 as: 0bA1A0
        # level_A0 = bool((input>>0) & 0b1)
        # level_A1 = bool((input>>1) & 0b1)

        ##Faster!
        # self.pin_2A0.value = self.enc_dict_fast[output_pos][1]
        # self.pin_2A1.value = self.enc_dict_fast[output_pos][0]

        ##Fastest!
        binary = Decimal_to_Bin(col_grp,2)
        self.pin_2A0.value = binary[1]
        self.pin_2A1.value = binary[0]

        ##Fastest!
        binary = Decimal_to_Bin(output_pos,4)
        col_demux.pin_A0.value = binary[3]
        col_demux.pin_A1.value = binary[2]
        col_demux.pin_A2.value = binary[1]
        col_demux.pin_A3.value = not (binary[0])     #On column: Connect to 2A/2B header pin 18. INVERTED (Not used for row)

    def set_output(self, ch, output_pos):
        #Set input pins so that E_row 'row' is enabled!

        output = ~(0b0001<<output_pos) & 0xF
        input = self.encode(output) #Input corresponds to the states of A0 and A1 as: 0bA1A0
        level_A0 = bool((input>>0) & 0b1)
        level_A1 = bool((input>>1) & 0b1)

        #enable gpios
        #print("74HC139 input:")
        #print('pin_{}A0={}'.format(ch,level_A0))
        #print('pin_{}A1={}'.format(ch,level_A1))

        if ch == 1:
            self.pin_1A0.value = inv_value(level_A0, self.pin_1A0_inv)
            self.pin_1A1.value = inv_value(level_A1, self.pin_1A1_inv)
        elif ch == 2:
            self.pin_2A0.value = inv_value(level_A0, self.pin_2A0_inv)
            self.pin_2A1.value = inv_value(level_A1, self.pin_2A1_inv)
        else:
            raise ValueError('invalid ch number')

    def enable_output(self, ch):

        #This is called when an enable of the pixels is wanted (after all setups)

        #Enable the outputs (set E pin to low -> set pin to high since this is inverted in 74HC02D)
        if ch == 1:
            self.pin_1E.value = True
            #self.pin_1E.value = inv_value(True, self.pin_1E_inv)
        elif ch == 2:
            self.pin_2E.value = True
            #self.pin_2E.value = inv_value(True, self.pin_2E_inv)
        else:
            raise ValueError('invalid ch number')


    def disable_output(self, ch):
        #Disable the outputs (set E pin to High -> set pin to low since this is inverted in 74HC02D)
        if ch == 1:
            self.pin_1E.value = inv_value(False, self.pin_1E_inv)
        elif ch == 2:
            self.pin_2E.value = inv_value(False, self.pin_2E_inv)
        else:
            raise ValueError('invalid ch number')

class FlipFlop:
    col_pin_map = {
    'input': {
        'A0': (digitalio.DigitalInOut(board.GP6), False),    #Connect to A0_cols header pin 13
        'A1': (digitalio.DigitalInOut(board.GP7), False),    #Connect to A1_cols header pin 14
        'A2': (digitalio.DigitalInOut(board.GP8), False),    #Connect to A2_cols header pin 15
        'A3': (digitalio.DigitalInOut(board.GP9), True),    #Connect to 2A/2B header pin 18. INVERTED
    },
    'output_r': {
        'C0_r': 1<<1,    #Q1
        'C1_r': 1<<2,    #Q2
        'C2_r': 1<<3,    #Q3
        'C3_r': 1<<4,    #Q4
        'C4_r': 1<<5,    #Q5
        'C5_r': 1<<6,    #Q6
        'C6_r': 1<<7,    #Q7
    },
    'output_s': {
        'C0_s': 1<<9,    #Q9
        'C1_s': 1<<10,    #Q10
        'C2_s': 1<<11,    #Q11
        'C3_s': 1<<12,    #Q12
        'C4_s': 1<<13,    #Q13
        'C5_s': 1<<14,    #Q14
        'C6_s': 1<<15,    #Q15
        }
    }

    col_grp_pin_map = {
        0: 1<<1,    #Q1
        1: 1<<2,    #Q2
        2: 1<<3,    #Q3
        3: 1<<4,    #Q4
        4: 1<<5,    #Q5
        5: 1<<6,    #Q6
        6: 1<<7,    #Q7
    }

    row_pin_map = {
        'input': {
            'A0': (digitalio.DigitalInOut(board.GP10), False),   #Connect to A0_rows header pin 5
            'A1': (digitalio.DigitalInOut(board.GP11), False),   #Connect to A1_rows header pin 7
            'A2': (digitalio.DigitalInOut(board.GP12), False),   #Connect to A2_rows header pin 6
            #'A3': (digitalio.DigitalInOut(board.GP13), False),   #not used?
        },
        'output_r': {
            'R0_r': 1<<1,    #Q1
            'R1_r': 1<<2,    #Q2
            'R2_r': 1<<3,    #Q3
            'R3_r': 1<<4,    #Q4
            'R4_r': 1<<5,    #Q5
            'R5_r': 1<<6,    #Q6
            'R6_r': 1<<7,    #Q7
        },
        'output_s': {
            'R0_s': 1<<9,    #Q9
            'R1_s': 1<<10,    #Q10
            'R2_s': 1<<11,    #Q11
            'R3_s': 1<<12,    #Q12
            'R4_s': 1<<13,    #Q13
            'R5_s': 1<<14,    #Q14
            'R6_s': 1<<15,    #Q15
            }
        }


    enable_pin_map = {
        'input': {
            '1A0': (digitalio.DigitalInOut(board.GP0), False),  #Connect to 1A0 header pin 8
            '1A1': (digitalio.DigitalInOut(board.GP1), False),  #Connect to 1A1 header pin 9
            '2A0': (digitalio.DigitalInOut(board.GP2), False),  #Connect to 2A0 header pin 16
            '2A1': (digitalio.DigitalInOut(board.GP3), False),  #Connect to 2A1 header pin 17
            '1E':  (digitalio.DigitalInOut(board.GP4), False),   #Connect to 1A and/or 1B header pin 10/11. Inverted! High for enable!  (Not really used when updating screen. Could be set high at init)
            '2E':  (digitalio.DigitalInOut(board.GP5), True),   #Connect to header pin 19 (controlled by a pulse curcuit with cap ensures short on-time) High for pulse / enable
        },
        'output_row': {
            'E_row0': 0,
            'E_row1': 1,
            'E_row2': 2,
            'E_row3': 3,
        },
        'output_col': {
            'E_col0': 0,
            'E_col1': 1,
            'E_col2': 2,
            'E_col3': 3,
        }
    }

    def __init__(self, flip_time, sweep_mode):
        self.enable = Demux_74HC139(self.enable_pin_map)
        self.col_demux = demux_74HC4514(self.col_pin_map)
        self.row_demux = demux_74HC4514(self.row_pin_map)

        self.flip_time = flip_time
        self.digit_w, self.digit_h = 28, 13

        #sweep_modes = {'row': 1, 'col': 2, 'diag': 3, 'random': 4}
        #choices = {'a': 1, 'b': 2}
        #result = choices.get('a', 'default')

        self.sweep_mode = sweep_mode

        #Set initial pixel state to all zeros
        self.pixel_state = [[0] * self.digit_w for _ in range(self.digit_h)]    #Pixel display state matrix as 2d list (13x28)


        #Row enable, this will be held enabled from now (maybe place in some init instead?)
        self.enable.enable_output(1)
        self.enable.pin_2E.value = 0
        time.sleep(self.flip_time)

        # Add some initiation that clears all pixels?

        # Set all pixels manual to zero
        col = list(range(0,28,1))
        row = list(range(0,13,1))

        for r in row:
            for c in col:
                self.set_pixel((r,c),0)     #(row,col)
                pass

        time.sleep(1)

    def update_display(self, display_data):

        flip_pixel = [[0] * self.digit_w for _ in range(self.digit_h)]
        flip_index_list = []

        #Compare incoming display_data with the display pixel state. Sort out the pixels that should change
        ##flip_index_list will be ordered by (row1,col1),(row1,col2),...,(row1,col28)
        for index_r, pixel_r in enumerate(display_data):
            for index_c, pixel_c in enumerate(display_data[index_r]):
                #pixel ^ self.pixel_state[]
                #print(display_data[index_r][index_c])
                if display_data[index_r][index_c] ^ self.pixel_state[index_r][index_c]:
                    flip_index_list.append((index_r,index_c))
                    flip_pixel[index_r][index_c] = display_data[index_r][index_c]


        # for r in range(len(flip_pixel)):
        #     print(flip_pixel[r])

        #Sort the pixel flip order according to sweep_mode
        if self.sweep_mode == 'row':
            pass                                                                #No need to sort since elements are already row-by-row according to above
        elif self.sweep_mode == 'col':
            flip_index_list = sorted(flip_index_list, key=lambda tup: tup[1])   #Sort by column (https://stackoverflow.com/questions/3121979/how-to-sort-a-list-tuple-of-lists-tuples-by-the-element-at-a-given-index)
        elif self.sweep_mode == 'diag':
            pass
        elif self.sweep_mode == 'random':
            flip_index_list = shuffled(flip_index_list)
        else:
            pass

        #Set pixels with set_pixel
        for ind in flip_index_list:
            self.set_pixel(ind, display_data[ind[0]][ind[1]])

        #Update display pixel state (some kind of state variable keeping track of which pixels are set)
        self.pixel_state = display_data

    def set_rows_cols(self, row_range : range, col_range : range, pixel_value):
        for r in row_range:
            for c in col_range:
                self.set_pixel((r,c),pixel_value)     #(row,col)

    #@micropython.native
    def set_pixel(self,pixel,value):

        #debug.value = debug.value ^ 1
        #Check row in pixel (tuple)
        row = pixel[0]
        row_grp = row // 7
        row_gpr_pixel = (row % 7) #+ (value*8)
        #print("row:")
        #print(row_grp,row_gpr_pixel)

        col = pixel[1]
        col_grp = col // 7
        col_gpr_pixel = (col % 7) #+ (value*8)
        #print("col:")
        #print(col_grp,col_gpr_pixel)

        #debug.value = debug.value ^ 1

        #---------Set enable group demux---------
        #key = 'E_row{}'.format(row_grp)             #This might take unneccesary long time?
        output_pos = row_gpr_pixel+1+(value*8)
        #print("output_pos:{}".format(output_pos))
        self.enable.set_row_output(row_grp, output_pos, self.row_demux)
        #self.enable.set_output(1,self.enable_pin_map['output_row'][key])

        #print(key)

        #key = 'E_col{}'.format(col_grp)
        output_pos = col_gpr_pixel+1+(value*8)
        #print("output_pos:{}".format(output_pos))
        self.enable.set_col_output(col_grp, output_pos, self.col_demux)
        #self.enable.set_output(2,self.enable_pin_map['output_col'][key])
        #time.sleep(0.01)
        #print(key)
        #----------------------------------------

        #debug.value = debug.value ^ 1

        #--------Set 74HC4514 (pixel) demux------
        #print("pixels:")

            # if value == 0:
            #     key_append = 'r'
            # elif value == 1:
            #     key_append = 's'
            # else:
            #     pass

            # key = 'C{}_{}'.format(col_gpr_pixel,key_append)
            # key_set_reset = 'output_{}'.format(key_append)
            # self.col_demux.set_output(self.col_pin_map[key_set_reset][key])

        #self.col_demux.set_output(col_gpr_pixel+1+(value*8))
            #self.col_demux.set_output(1<<(col_gpr_pixel+1+(value*8)))
            #print(key_set_reset, key)

            # key = 'R{}_{}'.format(row_gpr_pixel,key_append)
            # key_set_reset = 'output_{}'.format(key_append)
            # self.row_demux.set_output(self.row_pin_map[key_set_reset][key])

        #self.row_demux.set_output(row_gpr_pixel+1+(value*8))
            #self.row_demux.set_output(1<<(row_gpr_pixel+1+(value*8)))
            #print(key_set_reset, key)
            #----------------------------------------

            #debug.value = debug.value ^ 1

        #Set enable pins
        # #Col enable, this sends a pulse
        self.enable.pin_2E.value = 1
        #time.sleep(1)
        microcontroller.delay_us(3000)
        self.enable.pin_2E.value = 0

        #debug.value = debug.value ^ 1
        #print("set_pixel. Value:{}".format(value))


class Font:
    def __init__(self, file):
        self.file = file
        self.fnt_dict = {}

        self.read_font_file()

    def read_font_file(self):
        #----------Read font file------
        font_file = open("flip_dot\\{}".format(self.file), "r")

        a = font_file.read().splitlines()
        for r in range(len(a)):
            row = list(a[r].split(";"))
            key = row[0]
            value = row[1]

            char_list = []
            while value.find("]") > 0:
                ind = value.find("]")
                row_value = value[1:ind+1].strip('][').split(', ')
                char_list.append([int(i) for i in row_value])
                value = value[ind+3:]

            #Append key value pair to dict
            self.fnt_dict.setdefault(key, char_list)

        font_file.close()


class Pixel_Map:
    def __init__(self, size : tuple, margin : tuple):

        self.w = size[0]
        self.h = size[1]
        self.pixel_map = [[0] * self.w for _ in range(self.h)]    #Pixel display map matrix as 2d list (13x28)

        self.left_margin = margin[0]
        self.top_margin = margin[1]
        self.char_spacing = 1

    def read_pixel_file(self, filename):
        # Read file (should be done on Pico!)
        csv_file = open("flip_dot\\{}".format(filename), "r")

        a = csv_file.read().splitlines()
        data = []
        for r in range(len(a)):
            row = list(a[r].split(","))
            data.append([int(i) for i in row])

        self.pixel_map = data
        csv_file.close()

    #Generell metod för att fylla pixel_map. Om det går att ta in både chars och nummer?
    def fill_pixel_map(self, fnt_dict, text):
        col_offset = self.left_margin   #Current column position

        #Clear pixel_map???

        #Loop through all characters to get the tallest character, this will set the top margin
        char_h_max = 0
        for char in text:

            try:
                #See if the char exists in the font dict
                pixel_char = fnt_dict[char]
                #width_total = width_total + len(pixel_char[0])
                #save max length
                char_h_max = len(pixel_char) if len(pixel_char)>char_h_max else char_h_max

            except KeyError:    #Key not found in font dict
                print("Character '{}' does not exist in the selected font!".format(char))

            except:
                print("Invalid number! Cannot display!")

        # if(width_total > len(self.pixel_map[0])):
        #     print("Warning: The input text is too long for screen. Text will be cropped!")

        #Loop through all characters and create pixel map
        for char in text:

            try:
                #See if the char exists in the font dict
                pixel_char = fnt_dict[char]

                char_w = len(pixel_char[0])
                char_h = len(pixel_char)

                if (col_offset+char_w) >= len(self.pixel_map[0]):
                    print("Warning: The input text is too long for screen. Text will be cropped!")
                    char_w = len(self.pixel_map[0]) - col_offset

                row_offset = char_h_max-char_h + self.top_margin

                for r in range(0,char_h):
                    self.pixel_map[row_offset+r][col_offset:col_offset+char_w] = pixel_char[r][0:char_w]

                #increment col_offset
                col_offset += char_w + self.char_spacing

            except KeyError:    #Key not found in font dict
                print("Character '{}' does not exist in the selected font!".format(char))

            except:
                print("Invalid number! Cannot display!")

    def print_pixel_map(self):
        for r in range(len(self.pixel_map)):
            print(self.pixel_map[r])

data_2 = [
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0],
[0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
[0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
]

class Driver:
    def __init__(self):
        self.led = digitalio.DigitalInOut(board.LED)
        self.led.direction = digitalio.Direction.OUTPUT
        self.led.value = False

        self.pico_off = digitalio.DigitalInOut(board.GP15)
        self.pico_off.direction = digitalio.Direction.OUTPUT
        self.pico_off.value = False

        self.boost_en = digitalio.DigitalInOut(board.GP14)
        self.boost_en.direction = digitalio.Direction.OUTPUT
        self.boost_en.value = False

        self.board_on = digitalio.DigitalInOut(board.GP18)
        self.board_on.direction = digitalio.Direction.OUTPUT
        self.board_on.value = True
  

    def enable_24V(self):
        self.boost_en.value = True

    def disable_24V(self):
        self.boost_en.value = False

    def disable_pico(self):

        self.led.drive_mode = digitalio.DriveMode.OPEN_DRAIN
        self.led.value = True

        self.boost_en.drive_mode = digitalio.DriveMode.OPEN_DRAIN
        self.boost_en.value = True

        self.board_on.drive_mode = digitalio.DriveMode.OPEN_DRAIN
        self.board_on.value = True

        self.pico_off.value = True

    def enable_board_3V3(self):
        self.board_on.value = False

    def disable_board_3V3(self):
        self.board_on.value = True

def main():

    #print('Flip Flop asdf!')

    #time.sleep(5)
    driver = Driver()
    driver.enable_24V()
    driver.enable_board_3V3()
    driver.led.value = True
    print('24V and 3v3 enabled')
    time.sleep(5)

    ########## DATE TIME ##############
    # Using datetime.combine()
    #d = adafruit_datetime.date(2022, 12, 9)
    #t = adafruit_datetime.time(12, 30)
    #print(adafruit_datetime.datetime.combine(d, t))

    ###################################

    ############ FlipFlop Class #######
    flipflop = FlipFlop(1, 'row')   #3ms seems to be some kind of minimum delay.

    ###################################


    ############ Font Class #######
    #fnt = Font(file = "5x5_numbers.csv")
    fnt = Font(file = "seven_segment_3_nocorner.csv")
    ############ Pixel Class #######
    image_w = 28
    image_h = 13
    size = (image_w,image_h)

    margin_left = 0
    margin_top = 2
    margin = (margin_left,margin_top)

    pixels = Pixel_Map(size, margin)

    #pixels.fill_pixel_map(fnt.fnt_dict, "13:20")

    pixel_value = 0
    time_ns = 0

    #flipflop.update_display(pixels.pixel_map)
    cnt = 0

    while True:
        pixel_value = pixel_value ^ 1
        for row in range(0,13): #13
            #pixel_value = pixel_value ^ 1
            for col in range(0,28): #28
                flipflop.set_pixel((row,col),pixel_value)
                microcontroller.delay_us(1000)
                
        #flipflop.set_pixel((12,11),pixel_value)

        time.sleep(1)

    while cnt<10:
        ########Using datetime.now()########
        #print("Current time (GMT +1):", adafruit_datetime.datetime.now())
        #time_now = adafruit_datetime.datetime.now()
        #string_i_want=('%02d:%02d'%(time_now.minute,time_now.second))
        #print(string_i_want)
        #print(time_now.minute)
        #print(time_now.second)

        ##--------Speed test------------
        #pixel_value = pixel_value ^ 1
        #flipflop.set_pixel((0,0),pixel_value)
        #print(pixel_value)
        #flipflop.set_rows_cols(range(4,5), range(17,19), pixel_value)

        #---pixel test---#
    # pixel_value = 1
    # flipflop.set_pixel((0,0),pixel_value)

    #flipflop.update_display(pixels.pixel_map)
    # time.sleep(2)
    # pixel_value = 0
    # flipflop.set_pixel((0,0),pixel_value)
        pixel_value = pixel_value ^ 1
        for row in range(0,13): #13
            #pixel_value = pixel_value ^ 1
            for col in range(0,28): #28
                flipflop.set_pixel((row,col),pixel_value)
                #time.sleep(1)
                #pixel_value = pixel_value ^ 1

        time.sleep(3)
        cnt += 1

    driver.disable_24V()
    driver.disable_board_3V3()
    print('24V and 3v3 disabled')
    driver.led.value = False
    time.sleep(5)

    print('Shuttind down')
    #driver.disable_pico()

if __name__ == '__main__':
    main()

## Todo
#Check timing delay with random gpio and scope!