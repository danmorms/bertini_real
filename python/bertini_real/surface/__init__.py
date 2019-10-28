# Danielle Brake

# Foong Min Wong
# University of Wisconsin, Eau Claire
# Fall 2019

"""
This module contains Surface and Piece objects.

"""

import bertini_real.vertextype
import bertini_real.parse
import numpy as np
from bertini_real.decomposition import Decomposition
from bertini_real.curve import Curve



class Piece():
    """ Create a Piece object of a surface. A surface can be made of 1 piece or multiple pieces. """

    def __init__(self, indices, surface):
        """ Initialize a Piece object with corresponding indices and surface

            :param indices: A list of nonsingular pieces' indices
            :param surface: Surface data
        """

        self.indices = indices
        self.surface = surface

    def __str__(self):
        """ toString method for Piece """
        result = "piece with indices:\n"
        result += "{}".format(self.indices)
        return result

    def isCompact(self):
        """ Check whether a piece is:
            (1) compact (no edges touch the bounding sphere) 
            (2) non-compact (at least 1 edge touches bounding sphere)

            Examples:
            sphere: (1 piece) - compact
            dingdong: (2 pieces) - one compact, one not compact
            octdong: (2 pieces) - both compact
            whitney: (2 pieces) - both non-compact
            paraboloid: (1 piece) - non compact

        """

        # bounding sphere
        sphere_curve = self.surface.sphere_curve.sampler_data

        for ii in self.indices:
            face = self.surface.faces[ii]
            if face['system top'] == "input_surf_sphere":
                return False

            if face['system bottom'] == "input_surf_sphere":
                return False

        # compact
        return True


    # point_singularities
    # the points on a piece ,  left and right edge will be degenerated
    # type critical

    # tell whether the rank of Jacobian is deficient at point living on the

    # GOAL: list of singular points on the pieces

    # take system for the surface
    # take the jacobian system of the surface
    # evaluate the jacboian of the system at each critical point
    # if it is ranked-deficient, then that point is singular

    def point_singularities(self):
        """ Getter of singularity points from a Piece object """

        point_singularities = []

        pieces = self.surface.separate_into_nonsingular_pieces()
        print(pieces)

        # for ii in range(len(pieces)):
        #     for jj in pieces[ii].indices:
        #         print(self.surface.is_vertex_of_type(jj,self.vertex_type.singular))



        # pieces[0].is_vertex_of_type

        return point_singularities



class Surface(Decomposition):
    """ Create a Surface object based on the decomposition

        :param Decomposition: Decomposition data from decomp file

    """

    def __init__(self, directory):
        """ Initialize a Surface Object

            :param directory: Directory of the surface folder
        """
        self.directory = directory
        self.inputfilename = None
        self.num_variables = 0
        self.dimension = 9
        self.pi = []
        self.num_patches = 0
        self.patch = {}
        self.radius = 0
        self.center_size = 0
        self.center = []
        self.num_faces = 0
        self.num_midpoint_slices = 0
        self.num_critical_slices = 0
        self.num_singular_curves = 0
        self.singular_curve_multiplicities = []
        self.faces = {}    # stores all data from F.faces file
        self.midpoint_slices = []
        self.critical_point_slices = []
        self.critical_curve = []
        self.sphere_curve = []
        self.singular_curves = []
        self.singular_names = []
        self.surface_sampler_data = []   # store all surface_sampler data
        self.vertex_types_data = []
        self.input = None

        # automatically parse data files to gather curve data
        self.parse_decomp(self.directory)
        self.parse_surf(self.directory)
        self.parse_vertex_types(self.directory)
        self.gather_faces(self.directory)
        self.gather_curves(self.directory)
        try:
            self.gather_surface_samples(self.directory)
        except:
            print("no samples found")

        self.read_input(self.directory)

    def __str__(self):
        """ toString method for Surface """
        result = "surface with:\n"
        result += "{} faces".format(self.num_faces)
        return result

    def parse_surf(self, directory):
        """ Parse and store into surface data 
            
            :param directory: Directory of the surface folder
        """
        surf_data = bertini_real.parse.parse_Surf(directory)
        self.num_faces = surf_data[0]
        self.num_edges = surf_data[1]
        self.num_midpoint_slices = surf_data[2]
        self.num_critical_slices = surf_data[3]
        self.num_singular_curves = surf_data[4]
        self.singular_curve_multiplicities = surf_data[5]

    def parse_vertex_types(self, directory):
        """ Parse and store vertex types data

        :param directory: Directory of the surface folder
        """
        vertex_types_data = bertini_real.parse.parse_vertex_types(directory)
        self.vertex_types_data = vertex_types_data

    def gather_faces(self, directory):
        """ Gather the faces of surface
        
            :param directory: Directory of the surface folder
        """
        self.faces = bertini_real.parse.parse_Faces(directory)

    def gather_curves(self, directory):
        """ Gather the curves of surface
        
            :param directory: Directory of the surface folder
        """
        for ii in range(self.num_midpoint_slices):
            new_curve = Curve(directory + '/curve_midslice_' + str(ii))
            self.midpoint_slices.append(new_curve)
        for ii in range(self.num_critical_slices):
            new_curve = Curve(directory + '/curve_critslice_' + str(ii))
            self.critical_point_slices.append(new_curve)

        self.critical_curve = Curve(directory + '/curve_crit')
        self.sphere_curve = Curve(directory + '/curve_sphere')

        for ii in range(self.num_singular_curves):
            filename = directory + '/curve_singular_mult_' + \
                str(self.singular_curve_multiplicities[ii][0]) + '_' + str(
                    self.singular_curve_multiplicities[ii][1])
            new_curve = Curve(filename)
            self.singular_curves.append(new_curve)
            self.singular_names.append(new_curve.inputfilename)

    def gather_surface_samples(self, directory):
        """ Gather the surface samples of surface
        
            :param directory: Directory of the surface folder
        """
        self.surface_sampler_data = bertini_real.parse.parse_surface_Samples(
            directory)

    def is_vertex_of_type(self, index, type):
        """ Check if a vertex matches certain VertexType 

            :param index: Indices of a Piece object
            :param type: A VertexType
        """

        self.vertex_types_data = bertini_real.parse.parse_vertex_types(
            self.directory)
        # print(self.vertex_types_data)
        return bool((self.vertex_types_data[index] & type))

    def check_data(self):
        """ Check data """
        try:
            if self.dimension != 2:
                print('This function designed to work on surfaces decomposed with bertini_real.  your object has dimension ' + self.dimension)

        except:
            return

    def faces_nonsingularly_connected(self, seed_index):
        """ Compute the faces that are nonsingualrly connected

            :param seed_index: Index of seed
            :rtype: Two lists of connected and unconnected faces
        """
        self.check_data()

        new_indices = [seed_index]
        connected = []

        while not(new_indices == []):
            connected.extend(new_indices)
            new_indices = self.find_connected_faces(connected)

        connected.sort()
        set_num_faces = list(range(self.num_faces))

        unconnected = list(set(set_num_faces) - set(connected))

        return connected, unconnected

    def find_connected_faces(self, current):
        
        new_indices = []

        unexamined_indices = list(range(self.num_faces))

        unexamined_indices = list(set(unexamined_indices) - set(current))

        for ii in range(len(current)):
            c = current[ii]
            f = self.faces[c]  # unpack the current face
            deleteme = []

            for jj in range(len(unexamined_indices)):
                d = unexamined_indices[jj]
                g = self.faces[d]  # unpack the examined face

                if self.faces_nonsingularly_connect(f, g):
                    new_indices.append(d)
                    deleteme.append(d)

            unexamined_indices = list(set(unexamined_indices) - set(deleteme))

        return new_indices

    # queries whether faces f and g nonsingularly connect
    def faces_nonsingularly_connect(self, f, g):
        val = False  # assume no

        if self.cannot_possibly_meet(f, g):
            return val

        elif self.meet_at_left(f, g):
            val = True

        elif self.meet_at_right(f, g):
            val = True

        elif self.meet_at_top(f, g):
            val = True

        elif self.meet_at_bottom(f, g):
            val = True

        return val

    def cannot_possibly_meet(self, f, g):
        val = False

        if abs(f['middle slice index'] - g['middle slice index']) >= 2:
            val = True

        return val

    def meet_at_left(self, f, g):
        val = False

        for ii in range(f['num left']):
            e = self.critical_point_slices[
                f['middle slice index']].edges[f['left'][0]]
            a = e[2]

            for jj in range(g['num left']):
                E = self.critical_point_slices[
                    g['middle slice index']].edges[g['left'][0]]
                b = E[2]

                if a == b and not(self.is_degenerate(e)) and not(self.is_degenerate(E)):
                    val = True
                    return val

            for jj in range(g['num right']):
                E = self.critical_point_slices[
                    g['middle slice index'] + 1].edges[g['right'][0]]
                b = E[2]

                if a == b and not(self.is_degenerate(e)) and not(self.is_degenerate(E)):
                    val = True
                    return val
        return val

    def meet_at_right(self, f, g):
        val = False

        for ii in range(f['num right']):
            e = self.critical_point_slices[
                f['middle slice index'] + 1].edges[f['right'][0]]
            a = e[2]

            for jj in range(g['num left']):
                E = self.critical_point_slices[
                    g['middle slice index']].edges[g['left'][0]]
                b = E[2]

                if a == b and not(self.is_degenerate(e)) and not(self.is_degenerate(E)):
                    val = True
                    return val

            for jj in range(g['num right']):
                E = self.critical_point_slices[
                    g['middle slice index'] + 1].edges[g['right'][0]]
                b = E[2]

                if a == b and not(self.is_degenerate(e)) and not(self.is_degenerate(E)):
                    val = True
                    return val
        return val

    def meet_at_top(self, f, g):
        val = False

        if(f['system top'][0:15] == 'input_singcurve'):
            return val  # cannot meet singularly, because edge is singular
        else:
            # at least they are in the same interval
            if f['middle slice index'] != g['middle slice index']:
                return val

        if (f['system top'] == g['system top']):
            if (self.critical_curve.inputfilename == f['system top']):
                if (f['top'] == g['top']):
                    val = True
                    return val

        if (f['system top'] == g['system bottom']):
            if (self.critical_curve.inputfilename == f['system top']):
                if (f['top'] == g['bottom']):
                    val = True
                    return val

        return val

    def meet_at_bottom(self, f, g):
        """ Separate a surface into nonsingular pieces """
        val = False

        if(f['system bottom'][0:15] == 'input_singcurve'):
            return val  # cannot meet singularly, because edge is singular
        else:
            # at least they are in the same interval
            if f['middle slice index'] != g['middle slice index']:
                return val

        if (f['system bottom'] == g['system top']):
            if (self.critical_curve.inputfilename == f['system bottom']):
                if (f['bottom'] == g['top']):
                    val = True
                    return val

        if (f['system bottom'] == g['system bottom']):
            if (self.critical_curve.inputfilename == f['system bottom']):
                if (f['bottom'] == g['bottom']):
                    val = True
                    return val

        return val

    def is_degenerate(self, e):
        """ Check if  """
        val = (e[0] == e[1]) or (e[1] == e[2])
        return val

    def separate_into_nonsingular_pieces(self):
        """ Separate a surface into nonsingular pieces """

        self.check_data()

        pieces = []
        connected = []
        unconnected_this = [0]

        while not(unconnected_this == []):
            seed = unconnected_this[0]
            [connected_this, unconnected_this] = self.faces_nonsingularly_connected(
                seed)
            pieces.append(Piece(connected_this, self))
            connected.extend(connected_this)
            unconnected_this = list(set(unconnected_this) - set(connected))

        return pieces


def separate_into_nonsingular_pieces(data=None, directory='Dir_Name'):
    """ Separate a surface into nonsingular pieces

        :param data: Surface decomposition data. If data is None, then it reads the most recent BRData.pkl.
        :param directory: The directory of the surface
    """
    surface = Surface(data)
    surface.separate_into_nonsingular_pieces()
