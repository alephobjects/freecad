#  -*- coding: utf-8 -*-

# ***************************************************************************
# *                                                                         *
# *   Copyright (c) 2014 sliptonic <shopinthewoods@gmail.com>               *
# *                                                                         *
# *   This program is free software; you can redistribute it and/or modify  *
# *   it under the terms of the GNU Lesser General Public License (LGPL)    *
# *   as published by the Free Software Foundation; either version 2 of     *
# *   the License, or (at your option) any later version.                   *
# *   for detail see the LICENCE text file.                                 *
# *                                                                         *
# *   This program is distributed in the hope that it will be useful,       *
# *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
# *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
# *   GNU Library General Public License for more details.                  *
# *                                                                         *
# *   You should have received a copy of the GNU Library General Public     *
# *   License along with this program; if not, write to the Free Software   *
# *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
# *   USA                                                                   *
# *                                                                         *
# ***************************************************************************

import FreeCAD
import FreeCADGui
import Path
import PathGui
from PySide import QtCore, QtGui
import math
import DraftVecUtils as D
import PathScripts.PathUtils as P

"""Dragknife Dressup object and FreeCAD command"""

# Qt tanslation handling
try:
    _encoding = QtGui.QApplication.UnicodeUTF8

    def translate(context, text, disambig=None):
        return QtGui.QApplication.translate(context, text, disambig, _encoding)
except AttributeError:
    def translate(context, text, disambig=None):
        return QtGui.QApplication.translate(context, text, disambig)

movecommands = ['G1', 'G01', 'G2', 'G02', 'G3', 'G03']
rapidcommands = ['G0', 'G00']
arccommands = ['G2', 'G3', 'G02', 'G03']

currLocation = {}


class ObjectDressup:

    def __init__(self, obj):
        obj.addProperty("App::PropertyLink", "Base",  "Path", "The base path to modify")
        obj.addProperty("App::PropertyAngle", "filterangle", "Filter Angle", "Angles less than filter angle will not receive corner actions")
        obj.addProperty("App::PropertyFloat", "offset", "Offset", "Distance the point trails behind the spindle")
        obj.addProperty("App::PropertyFloat", "pivotheight", "Pivot Height", "Height to raise during corner action")

        obj.Proxy = self

    def __getstate__(self):
        return None

    def __setstate__(self, state):
        return None

    def getIncidentAngle(self, queue):
        global currLocation
        '''returns in the incident angle in radians between the current and previous moves'''
        # get the vector of the last move
        if queue[1].Name in arccommands:
            print queue
            print currLocation
            arcLoc = FreeCAD.Base.Vector(queue[2].X + queue[1].I, queue[2].Y + queue[1].J, currLocation['Z'])
            radvector = queue[1].Placement.Base.sub(arcLoc)  # vector of chord from center to point
            # vector of line perp to chord.
            v1 = radvector.cross(FreeCAD.Base.Vector(0, 0, 1))
        else:
            v1 = queue[1].Placement.Base.sub(queue[2].Placement.Base)

        # get the vector of the current move
        if queue[0].Name in arccommands:
            arcLoc = FreeCAD.Base.Vector((queue[1].x + queue[0].I), (queue[1].y + queue[0].J), currLocation['Z'])
            radvector = queue[1].Placement.Base.sub(arcLoc)  # calculate arcangle

            v2 = radvector.cross(FreeCAD.Base.Vector(0, 0, 1))

            # if switching between G2 and G3, reverse orientation
            if queue[1].Name in arccommands:
                if queue[0].Name != queue[1].Name:
                    v2 = D.rotate2D(v2, math.radians(180))
        else:
            v2 = queue[0].Placement.Base.sub(queue[1].Placement.Base)

        incident_angle = D.angle(v1, v2, FreeCAD.Base.Vector(0, 0, -1))
        return incident_angle

    def arcExtension(self, obj, queue):
        '''returns gcode for arc extension'''
        global currLocation
        results = []

        offset = obj.offset
        # Find the center of the old arc
        C = FreeCAD.Base.Vector(queue[2].x + queue[1].I, queue[2].y + queue[1].J, currLocation['Z'])

        # Find radius of old arc
        R = math.hypot(queue[1].I, queue[1].J)

        # Find angle subtended by the extension arc
        theta = math.atan2(queue[1].y - C.y, queue[1].x - C.x)
        if queue[1].Name in ["G2", "G02"]:
            theta = theta - offset / R
        else:
            theta = theta + offset / R

        # XY coordinates of new arc endpoint.
        Bx = C.x + R * math.cos(theta)
        By = C.y + R * math.sin(theta)

        # endpoint = FreeCAD.Base.Vector(Bx, By, currLocation["Z"])
        startpoint = queue[1].Placement.Base
        offsetvector = C.sub(startpoint)

        I = offsetvector.x
        J = offsetvector.y

        extend = Path.Command(queue[1].Name, {"I": I, "J": J, "X": Bx, "Y": By})
        results.append(extend)
        currLocation.update(extend.Parameters)

        replace = None
        return (results, replace)

    def arcTwist(self, obj, queue, lastXY, twistCW=False):
        '''returns gcode to do an arc move toward an arc to perform
        a corner action twist. Inclues lifting and plungeing the knife'''

        global currLocation
        pivotheight = obj.pivotheight
        offset = obj.offset
        results = []

        # set the correct twist command
        if twistCW is False:
            arcdir = "G3"
        else:
            arcdir = "G2"

        # move to the pivot heigth
        zdepth = currLocation["Z"]
        retract = Path.Command("G0", {"Z": pivotheight})
        results.append(retract)
        currLocation.update(retract.Parameters)

        # get the center of the destination arc
        arccenter = FreeCAD.Base.Vector(queue[1].x + queue[0].I, queue[1].y + queue[0].J, currLocation["Z"])

        # The center of the twist arc is the old line end point.
        C = queue[1].Placement.Base

        # Find radius of old arc
        R = math.hypot(queue[0].I, queue[0].J)

        # find angle of original center to startpoint
        v1 = queue[1].Placement.Base.sub(arccenter)
        segAngle = D.angle(v1, FreeCAD.Base.Vector(1, 0, 0), FreeCAD.Base.Vector(0, 0, -1))

        # Find angle subtended by the offset
        theta = offset / R

        # add or subtract theta depending on direction
        if queue[1].Name in ["G2", "G02"]:
            newangle = segAngle + theta
        else:
            newangle = segAngle - theta

        # calculate endpoints
        Bx = arccenter.x + R * math.cos(newangle)
        By = arccenter.y + R * math.sin(newangle)
        endpointvector = FreeCAD.Base.Vector(Bx, By, currLocation['Z'])

        # calculate IJ offsets of twist arc from current position.
        offsetvector = C.sub(lastXY)
        # I = offsetvector.x
        # J = offsetvector.y

        # add G2/G3 move
        arcmove = Path.Command(
        arcdir, {"X": endpointvector.x, "Y": endpointvector.y, "I": offsetvector.x, "J": offsetvector.y})
        results.append(arcmove)
        currLocation.update(arcmove.Parameters)

        # plunge back to depth
        plunge = Path.Command("G1", {"Z": zdepth})
        results.append(plunge)
        currLocation.update(plunge.Parameters)

        # The old arc move won't work so calculate a replacement command
        offsetv = arccenter.sub(endpointvector)

        # I = offsetv.x
        # J = offsetv.y

        replace = Path.Command(
            queue[0].Name, {"X": queue[0].X, "Y": queue[0].Y, "I": offsetv.x, "J": offsetv.y})

        return (results, replace)

    def lineExtension(self, obj, queue):
        '''returns gcode for line extension'''
        global currLocation

        offset = float(obj.offset)
        results = []

        v1 = queue[1].Placement.Base.sub(queue[2].Placement.Base)

        # extend the current segment to comp for offset
        segAngle = D.angle(v1, FreeCAD.Base.Vector(1, 0, 0), FreeCAD.Base.Vector(0, 0, -1))
        xoffset = math.cos(segAngle) * offset
        yoffset = math.sin(segAngle) * offset

        newX = currLocation["X"] + xoffset
        newY = currLocation["Y"] + yoffset

        extendcommand = Path.Command('G1', {"X": newX, "Y": newY})
        results.append(extendcommand)

        currLocation.update(extendcommand.Parameters)

        replace = None
        return (results, replace)

    def lineTwist(self, obj, queue, lastXY, twistCW=False):
        '''returns gcode to do an arc move toward a line to perform
        a corner action twist. Includes lifting and plungeing the knife'''
        global currLocation
        pivotheight = obj.pivotheight
        offset = obj.offset

        results = []

        # set the correct twist command
        if twistCW is False:
            arcdir = "G3"
        else:
            arcdir = "G2"

        # move to pivot height
        zdepth = currLocation["Z"]
        retract = Path.Command("G0", {"Z": pivotheight})
        results.append(retract)
        currLocation.update(retract.Parameters)

        C = queue[1].Placement.Base

        # get the vectors between endpoints to calculate twist
        v2 = queue[0].Placement.Base.sub(queue[1].Placement.Base)

        # calc arc endpoints to twist to
        segAngle = D.angle(v2, FreeCAD.Base.Vector(1, 0, 0), FreeCAD.Base.Vector(0, 0, -1))
        xoffset = math.cos(segAngle) * offset
        yoffset = math.sin(segAngle) * offset
        newX = queue[1].x + xoffset
        newY = queue[1].y + yoffset

        offsetvector = C.sub(lastXY)
        I = offsetvector.x
        J = offsetvector.y

        # add the arc move
        arcmove = Path.Command(
            arcdir, {"X": newX, "Y": newY, "I": I, "J": J})  # add G2/G3 move
        results.append(arcmove)

        currLocation.update(arcmove.Parameters)

        # plunge back to depth
        plunge = Path.Command("G1", {"Z": zdepth})
        results.append(plunge)
        currLocation.update(plunge.Parameters)

        replace = None
        return (results, replace)

    def execute(self, obj):
        newpath = []
        global currLocation

        if not obj.Base:
            return

        if not obj.Base.isDerivedFrom("Path::Feature"):
            return

        if obj.Base.Path.Commands:

            firstmove = Path.Command("G0", {"X": 0, "Y": 0, "Z": 0})
            currLocation.update(firstmove.Parameters)

            queue = []

            for curCommand in obj.Base.Path.Commands:
                replace = None
                # don't worry about non-move commands, just add to output
                if curCommand.Name not in movecommands + rapidcommands:
                    newpath.append(curCommand)
                    continue

                # rapid retract triggers exit move, else just add to output
                if curCommand.Name in rapidcommands:
                    if (curCommand.z > obj.pivotheight) and (len(queue) == 3):
                        # Process the exit move
                        tempqueue = queue
                        tempqueue.insert(0, curCommand)

                        if queue[1].Name in ['G01', 'G1']:
                            temp = self.lineExtension(obj, tempqueue)
                            newpath.extend(temp[0])
                            lastxy = temp[0][-1].Placement.Base
                        elif queue[1].Name in arccommands:
                            temp = self.arcExtension(obj, tempqueue)
                            newpath.extend(temp[0])
                            lastxy = temp[0][-1].Placement.Base

                    newpath.append(curCommand)
                    currLocation.update(curCommand.Parameters)
                    queue = []
                    continue

                # keep a queue of feed moves and check for needed corners
                if curCommand.Name in movecommands:
                    changedXYFlag = False
                    if queue:
                        if (curCommand.x != queue[0].x) or (curCommand.y != queue[0].y):
                            queue.insert(0, curCommand)
                            if len(queue) > 3:
                                queue.pop()
                            changedXYFlag = True
                    else:
                        queue = [curCommand]

                    # vertical feeding to depth
                    if curCommand.z != currLocation["Z"]:
                        newpath.append(curCommand)
                        currLocation.update(curCommand.Parameters)
                        continue

                    # Corner possibly needed
                    if changedXYFlag and (len(queue) == 3):

                        # check if the inciden angle incident exceeds the filter
                        incident_angle = math.degrees(self.getIncidentAngle(queue))

                        if abs(incident_angle) >= obj.filterangle:
                            if incident_angle >= 0:
                                twistCW = True
                            else:
                                twistCW = False
                            #
                            #  DO THE EXTENSION
                            #
                            if queue[1].Name in ['G01', 'G1']:
                                temp = self.lineExtension(obj, queue)
                                newpath.extend(temp[0])
                                replace = temp[1]
                                lastxy = temp[0][-1].Placement.Base
                            elif queue[1].Name in arccommands:
                                temp = self.arcExtension(obj, queue)
                                newpath.extend(temp[0])
                                replace = temp[1]
                                lastxy = temp[0][-1].Placement.Base
                            else:
                                FreeCAD.Console.PrintWarning("I don't know what's up")
                            #
                            #  DO THE TWIST
                            #
                            if queue[0].Name in ['G01', 'G1']:
                                temp = self.lineTwist(obj, queue, lastxy, twistCW)
                                replace = temp[1]
                                newpath.extend(temp[0])
                            elif queue[0].Name in arccommands:
                                temp = self.arcTwist(obj, queue, lastxy, twistCW)
                                replace = temp[1]
                                newpath.extend(temp[0])
                            else:
                                FreeCAD.Console.PrintWarning("I don't know what's up")
                    if replace is None:
                        newpath.append(curCommand)
                    else:
                        newpath.append(replace)
                    currLocation.update(curCommand.Parameters)
                    continue

            commands = newpath
            path = Path.Path(commands)
            obj.Path = path


class ViewProviderDressup:

    def __init__(self, vobj):
        vobj.Proxy = self

    def attach(self, vobj):
        self.Object = vobj.Object
        return


    def unsetEdit(self, vobj, mode=0):
        return False

    def setEdit(self, vobj, mode=0):
        return True

    def claimChildren(self):
        for i in self.Object.Base.InList:
            if hasattr(i, "Group"):
                group = i.Group
                for g in group:
                    if g.Name == self.Object.Base.Name:
                        group.remove(g)
                i.Group = group
                print i.Group
        #FreeCADGui.ActiveDocument.getObject(obj.Base.Name).Visibility = False
        return [self.Object.Base]

    def __getstate__(self):
        return None

    def __setstate__(self, state):
        return None


class CommandDragknifeDressup:

    def GetResources(self):
        return {'Pixmap': 'Path-Dressup',
                'MenuText': QtCore.QT_TRANSLATE_NOOP("DragKnife_Dressup", "DragKnife Dress-up"),
                'Accel': "P, S",
                'ToolTip': QtCore.QT_TRANSLATE_NOOP("DragKnife_Dressup", "Modifies a path to add dragknife corner actions")}

    def IsActive(self):
        return FreeCAD.ActiveDocument is not None

    def Activated(self):

        # check that the selection contains exactly what we want
        selection = FreeCADGui.Selection.getSelection()
        if len(selection) != 1:
            FreeCAD.Console.PrintError(
                translate("DragKnife_Dressup", "Please select one path object\n"))
            return
        if not selection[0].isDerivedFrom("Path::Feature"):
            FreeCAD.Console.PrintError(
                translate("DragKnife_Dressup", "The selected object is not a path\n"))
            return
        if selection[0].isDerivedFrom("Path::FeatureCompoundPython"):
            FreeCAD.Console.PrintError(
                translate("DragKnife_Dressup", "Please select a Path object"))
            return

        # everything ok!
        FreeCAD.ActiveDocument.openTransaction(translate("DragKnife_Dressup", "Create Dress-up"))
        FreeCADGui.addModule("PathScripts.DragknifeDressup")
        FreeCADGui.addModule("PathScripts.PathUtils")
        FreeCADGui.doCommand('obj = FreeCAD.ActiveDocument.addObject("Path::FeaturePython","DragknifeDressup")')
        FreeCADGui.doCommand('PathScripts.DragknifeDressup.ObjectDressup(obj)')
        FreeCADGui.doCommand('obj.Base = FreeCAD.ActiveDocument.' + selection[0].Name)
        FreeCADGui.doCommand('PathScripts.DragknifeDressup.ViewProviderDressup(obj.ViewObject)')
        FreeCADGui.doCommand('PathScripts.PathUtils.addToProject(obj)')
        FreeCADGui.doCommand('Gui.ActiveDocument.getObject(obj.Base.Name).Visibility = False')
        FreeCADGui.doCommand('obj.filterangle = 20')
        FreeCADGui.doCommand('obj.offset = 2')
        FreeCADGui.doCommand('obj.pivotheight = 4')

        FreeCAD.ActiveDocument.commitTransaction()
        FreeCAD.ActiveDocument.recompute()


if FreeCAD.GuiUp:
    # register the FreeCAD command
    FreeCADGui.addCommand('DragKnife_Dressup', CommandDragknifeDressup())

FreeCAD.Console.PrintLog("Loading DragKnife_Dressup... done\n")
