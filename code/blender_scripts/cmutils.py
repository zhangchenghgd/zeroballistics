
import struct
import math


#------------------------------------------------------------------------------
def writeUnsigned32(file, i):
    file.write(struct.pack("I", i))

#------------------------------------------------------------------------------
def writeUnsigned16(file, i):
    file.write(struct.pack("H", i))


#------------------------------------------------------------------------------
def writeFloat(file, f):
    file.write(struct.pack("f", f))


#------------------------------------------------------------------------------
def writeString(file, str):
    writeUnsigned32(file, len(str))
    file.write(str)
#    file.write("\0")    


#------------------------------------------------------------------------------
def writeVector(file, vector):
    file.write(struct.pack("3f", vector[0], vector[2], -vector[1]))

#------------------------------------------------------------------------------
def writeTexcoord(file, texcoord):
    file.write(struct.pack("2f", texcoord[0], texcoord[1]))

#------------------------------------------------------------------------------
def writeColor(file, c):
    file.write(struct.pack("4f", c[0], c[1], c[2], c[3]))
    
#------------------------------------------------------------------------------
#  Take into account different coordinate system.
#
#  In stunts, x is right, y is up, -z is forward (OpenGL camera system)
#  Matrices are stored column-major
#  In Blender, x is right, y is forward, z is up.
#  First index is col, second is row
#  Translation comp is 30,31,32
#  
def writeMatrix(file, matrix):
    file.write(struct.pack("16f",
                           matrix[0][0],  matrix[0][2],-matrix[0][1], matrix[0][3],
                           matrix[2][0],  matrix[2][2],-matrix[2][1], matrix[2][3],
                          -matrix[1][0], -matrix[1][2], matrix[1][1], matrix[1][3],
                           matrix[3][0],  matrix[3][2],-matrix[3][1], 1)) # XXXX there are situations where this is 0




    
#------------------------------------------------------------------------------
def areFloatsEqual(f1, f2):
#    return f1 == f2
    return abs(f1-f2) < 0.001

#------------------------------------------------------------------------------
def areVectorsEqual(v1, v2):
    return areFloatsEqual(v1[0], v2[0]) and areFloatsEqual(v1[1], v2[1]) and areFloatsEqual(v1[2], v2[2])


#------------------------------------------------------------------------------
def areColorsEqual(c1, c2):
    if c1 == None and c2 == None:
        return 1
    else:
        return areFloatsEqual(c1[0], c2[0]) and areFloatsEqual(c1[1], c2[1]) and \
               areFloatsEqual(c1[2], c2[2]) and areFloatsEqual(c1[3], c2[3])
    
    
#------------------------------------------------------------------------------
def areTexcoordsEqual(t1, t2):
    if t1 == None and t2 == None:
        return 1
    else:
        return areFloatsEqual(t1[0], t2[0]) and areFloatsEqual(t1[1], t2[1])

#------------------------------------------------------------------------------
def vecCross(v1, v2):
    res = []
    res.append(v1[1]*v2[2] - v1[2]*v2[1])
    res.append(v1[2]*v2[0] - v1[0]*v2[2])
    res.append(v1[0]*v2[1] - v1[1]*v2[0])
    return res

#------------------------------------------------------------------------------
def vecNormalize(v):
    length = math.sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2])
    if length == 0:
        print "Warning: vecNormalize with length zero"
    else:
        inv_length = 1.0 / length
        v[0] = v[0] * inv_length
        v[1] = v[1] * inv_length
        v[2] = v[2] * inv_length
