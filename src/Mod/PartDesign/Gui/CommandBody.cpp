/***************************************************************************
 *  Copyright (C) 2015 Alexander Golubev (Fat-Zer) <fatzer2@gmail.com>     *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"

#ifndef _PreComp_
# include <QApplication>
# include <QMessageBox>
# include <QInputDialog>
#endif

#include <Base/Console.h>
#include <App/Part.h>
#include <Gui/Command.h>
#include <Gui/Application.h>
#include <Gui/ActiveObjectList.h>
#include <Gui/MainWindow.h>
#include <Gui/MDIView.h>

#include <Mod/Sketcher/App/SketchObject.h>
#include <Mod/PartDesign/App/Body.h>
#include <Mod/PartDesign/App/Feature.h>
#include <Mod/PartDesign/App/FeatureSketchBased.h>

#include "Utils.h"
#include "WorkflowManager.h"


//===========================================================================
// Shared functions
//===========================================================================

namespace PartDesignGui {

/// Returns active part, if there is no such, creates a new part, if it fails, shows a message
App::Part* assertActivePart () {
    App::Part* rv= Gui::Application::Instance->activeView()->getActiveObject<App::Part *> ( PARTKEY );

    if ( !rv ) {
        Gui::CommandManager &rcCmdMgr = Gui::Application::Instance->commandManager();
        rcCmdMgr.runCommandByName("PartDesign_Part");
        rv = Gui::Application::Instance->activeView()->getActiveObject<App::Part *> ( PARTKEY );
        if ( !rv ) {
            QMessageBox::critical ( 0, QObject::tr( "Part creation failed" ),
                    QObject::tr( "Failed to create a part object." ) );
        }
    }

    return rv;
}

} /* PartDesignGui */

//===========================================================================
// PartDesign_Part
//===========================================================================
DEF_STD_CMD_A(CmdPartDesignPart);

CmdPartDesignPart::CmdPartDesignPart()
  : Command("PartDesign_Part")
{
    sAppModule    = "PartDesign";
    sGroup        = QT_TR_NOOP("PartDesign");
    sMenuText     = QT_TR_NOOP("Create part");
    sToolTipText  = QT_TR_NOOP("Create a new part and make it active");
    sWhatsThis    = sToolTipText;
    sStatusTip    = sToolTipText;
    sPixmap       = "Tree_Annotation";
}

void CmdPartDesignPart::activated(int iMsg)
{
    if ( !PartDesignGui::assureModernWorkflow( getDocument() ) )
        return;

    openCommand("Add a part");
    std::string FeatName = getUniqueObjectName("Part");

    std::string PartName;
    PartName = getUniqueObjectName("Part");
    doCommand(Doc,"App.activeDocument().Tip = App.activeDocument().addObject('App::Part','%s')",PartName.c_str());
    // TODO We really must to set label ourselfs? (2015-08-17, Fat-Zer)
    doCommand(Doc,"App.activeDocument().%s.Label = '%s'", PartName.c_str(),
            QObject::tr(PartName.c_str()).toUtf8().data());
    doCommand(Gui::Command::Gui, "Gui.activeView().setActiveObject('%s', App.activeDocument().%s)",
            PARTKEY, PartName.c_str());

    updateActive();
}

bool CmdPartDesignPart::isActive(void)
{
    return hasActiveDocument() && !PartDesignGui::isLegacyWorkflow ( getDocument () );
}

//===========================================================================
// PartDesign_Body
//===========================================================================
DEF_STD_CMD_A(CmdPartDesignBody);

CmdPartDesignBody::CmdPartDesignBody()
  : Command("PartDesign_Body")
{
    sAppModule    = "PartDesign";
    sGroup        = QT_TR_NOOP("PartDesign");
    sMenuText     = QT_TR_NOOP("Create body");
    sToolTipText  = QT_TR_NOOP("Create a new body and make it active");
    sWhatsThis    = sToolTipText;
    sStatusTip    = sToolTipText;
    sPixmap       = "PartDesign_Body_Create_New";
}

void CmdPartDesignBody::activated(int iMsg)
{
    if ( !PartDesignGui::assureModernWorkflow( getDocument() ) )
        return;
    App::Part *actPart = PartDesignGui::getActivePart ();
    App::Part* partOfBaseFeature = nullptr;

    std::vector<App::DocumentObject*> features =
        getSelection().getObjectsOfType(Part::Feature::getClassTypeId());
    App::DocumentObject* baseFeature = nullptr;


    if (!features.empty()) {
        if (features.size() == 1) {
            baseFeature = features[0];
            if ( baseFeature->isDerivedFrom ( PartDesign::Feature::getClassTypeId() ) &&
                    PartDesign::Body::findBodyOf ( baseFeature ) ) {
                // Prevent creating bodies based on features already belonging to other bodies
                QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Bad base feature"),
                        QObject::tr("Body can't be based on a PartDesign feature."));
                baseFeature = nullptr;
            }
            else if (PartDesign::Body::findBodyOf ( baseFeature )){
                QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Bad base feature"),
                        QObject::tr("%1 already belongs to a body, can't use it as base feature for another body.")
                                     .arg(QString::fromUtf8(baseFeature->Label.getValue())));
                baseFeature = nullptr;
            }
            else if ( baseFeature->isDerivedFrom ( Part::BodyBase::getClassTypeId() ) )  {
                // Prevent creating bodies based on bodies
                QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Bad base feature"),
                        QObject::tr("Body can't be based on annother body."));
                baseFeature = nullptr;
            } else {

                partOfBaseFeature = App::Part::getPartOfObject(baseFeature);
                if (partOfBaseFeature != 0  &&  partOfBaseFeature != actPart){
                    //prevent cross-part mess
                    QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Bad base feature"),
                            QObject::tr("Base feature (%1) belongs to other part.")
                                         .arg(QString::fromUtf8(baseFeature->Label.getValue())));
                    baseFeature = nullptr;
                };
            }

        } else {
            QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Bad base feature"),
                QObject::tr("Body may be based no more than on one feature."));
            return;
        }
    }


    openCommand("Add a Body");

    std::string bodyName = getUniqueObjectName("Body");

    // add the Body feature itself, and make it active
    doCommand(Doc,"App.activeDocument().addObject('PartDesign::Body','%s')", bodyName.c_str());
    if (baseFeature) {
        if (partOfBaseFeature){
            //withdraw base feature from Part, otherwise visibility mandess results
            doCommand(Doc,"App.activeDocument().%s.removeObject(App.activeDocument().%s)",
                    partOfBaseFeature->getNameInDocument(), baseFeature->getNameInDocument());
        }
        doCommand(Doc,"App.activeDocument().%s.BaseFeature = App.activeDocument().%s",
                bodyName.c_str(), baseFeature->getNameInDocument());
    }
    addModule(Gui,"PartDesignGui"); // import the Gui module only once a session
    doCommand(Gui::Command::Gui, "Gui.activeView().setActiveObject('%s', App.activeDocument().%s)", 
            PDBODYKEY, bodyName.c_str());

    // Make the "Create sketch" prompt appear in the task panel
    doCommand(Gui,"Gui.Selection.clearSelection()");
    doCommand(Gui,"Gui.Selection.addSelection(App.ActiveDocument.%s)", bodyName.c_str());
    if (actPart) {
        doCommand(Doc,"App.activeDocument().%s.addObject(App.ActiveDocument.%s)",
                 actPart->getNameInDocument(), bodyName.c_str());
    }


    updateActive();
}

bool CmdPartDesignBody::isActive(void)
{
    return hasActiveDocument() && !PartDesignGui::isLegacyWorkflow ( getDocument () );
}

//===========================================================================
// PartDesign_Migrate
//===========================================================================

DEF_STD_CMD_A(CmdPartDesignMigrate);

CmdPartDesignMigrate::CmdPartDesignMigrate()
  : Command("PartDesign_Migrate")
{
    sAppModule    = "PartDesign";
    sGroup        = QT_TR_NOOP("PartDesign");
    sMenuText     = QT_TR_NOOP("Migrate");
    sToolTipText  = QT_TR_NOOP("Migrate document to the modern partdesign workflow");
    sWhatsThis    = sToolTipText;
    sStatusTip    = sToolTipText;
}

void CmdPartDesignMigrate::activated(int iMsg)
{
    App::Document *doc = getDocument();

    std::set<PartDesign::Feature*> migrateFeatures;


    // Retrive all PartDesign Features objects and filter out features already belongs to some body
    for ( const auto & feat: doc->getObjects(  ) ) {
         if( feat->isDerivedFrom( PartDesign::Feature::getClassTypeId() ) &&
                 !PartDesign::Body::findBodyOf( feat ) && PartDesign::Body::isSolidFeature ( feat ) ) {
             migrateFeatures.insert ( static_cast <PartDesign::Feature *>( feat ) );
         }
    }

    if ( migrateFeatures.empty() ) {
        if ( !PartDesignGui::isModernWorkflow ( doc ) ) {
            // If there is nothing to migrate and workflow is still old just set it to modern
            PartDesignGui::WorkflowManager::instance()->forceWorkflow (
                    doc, PartDesignGui::Workflow::Modern );
        } else {
            // Huh? nothing to migrate?
            QMessageBox::warning ( 0, QObject::tr ( "Nothing to migrate" ),
                    QObject::tr ( "No PartDesign features which doesn't belong to a body found."
                        " Nothing to migrate." ) );
        }
        return;
    }

    // Note: this action is undoable, should it be?
    PartDesignGui::WorkflowManager::instance()->forceWorkflow ( doc, PartDesignGui::Workflow::Modern );

    // Put features into chains. Each chain should become a separate body.
    std::list< std::list<PartDesign::Feature *> > featureChains;
    std::list<PartDesign::Feature *> chain; //< the current chain we are working on

    for (auto featIt = migrateFeatures.begin(); !migrateFeatures.empty(); ) {
        Part::Feature *base = (*featIt)->getBaseObject( /*silent =*/ true );

        chain.push_front ( *featIt );

        if ( !base || !base->isDerivedFrom (PartDesign::Feature::getClassTypeId () ) ||
                PartDesignGui::isAnyNonPartDesignLinksTo ( static_cast <PartDesign::Feature *>(base),
                                                           /*respectGroups=*/ true ) ) {
            // a feature based on nothing as well as on non-partdesign solid starts a new chain
            auto newChainIt = featureChains.emplace (featureChains.end());
            newChainIt->splice (newChainIt->end(), chain);
        } else {
            // we are basing on some partdesign feature wich supposed to belong to some body
            PartDesign::Feature *baseFeat = static_cast <PartDesign::Feature *>( base );

            auto baseFeatSetIt = find ( migrateFeatures.begin (), migrateFeatures.end (), baseFeat );

            if ( baseFeatSetIt != migrateFeatures.end() ) {
                // base feature is pending for migration, switch to it and continue over
                migrateFeatures.erase(featIt);
                featIt = baseFeatSetIt;
                continue;
            } else {
                // The base feature seems already assigned to some chain
                // Find which
                std::list<PartDesign::Feature *>::iterator baseFeatIt;
                auto chainIt = std::find_if( featureChains.begin(), featureChains.end(),
                        [baseFeat, &baseFeatIt] ( std::list<PartDesign::Feature *>&chain ) mutable -> bool {
                            baseFeatIt = std::find( chain.begin(), chain.end(), baseFeat );
                            return baseFeatIt !=  chain.end();
                        } );

                if ( chainIt != featureChains.end() ) {
                    assert (baseFeatIt != chainIt->end());
                    if ( std::next ( baseFeatIt ) == chainIt->end() ) {
                        // just append our chain to already found
                        chainIt->splice ( chainIt->end(), chain );
                        // TODO If we will hit a third part everything will be messed up again.
                        //      Probably it will require a yet another smart-ass find_if. (2015-08-10, Fat-Zer)
                    } else {
                        // We have a fork of a partDesign feature here
                        // add a chain for current body
                        auto newChainIt = featureChains.emplace (featureChains.end());
                        newChainIt->splice (newChainIt->end(), chain);
                        // add a chain for forked one
                        newChainIt = featureChains.emplace (featureChains.end());
                        newChainIt->splice (newChainIt->end(), *chainIt,
                                std::next ( baseFeatIt ), chainIt->end());
                    }
                } else {
                    // The feature is not present in list pending for migration,
                    // This generally shouldn't happen but may be if we run into some broken file
                    // Try to find out the body we should insert into
                    // TODO Some error/warning is needed here (2015-08-10, Fat-Zer)
                    auto newChainIt = featureChains.emplace (featureChains.end());
                    newChainIt->splice (newChainIt->end(), chain);
                }
            }
        }
        migrateFeatures.erase ( featIt );
        featIt = migrateFeatures.begin ();
        // TODO Align visability (2015-08-17, Fat-Zer)
    } /* for */

    // TODO make it work without parts (2015-09-04, Fat-Zer)
    // add a part if there is no active yet
    App::Part *actPart = PartDesignGui::assertActivePart ();

    if (!actPart) {
        return;
    }

    // do the actual migration
    Gui::Command::openCommand("Migrate legacy part design features to Bodies");

    for ( auto chainIt = featureChains.begin(); !featureChains.empty();
            featureChains.erase (chainIt), chainIt = featureChains.begin () ) {
#ifndef FC_DEBUG
        if ( chainIt->empty () ) { // prevent crash in release in case of errors
            continue;
        }
#else
        assert ( !chainIt->empty () );
#endif
        Part::Feature *base = chainIt->front()->getBaseObject ( /*silent =*/ true );

        // Find a suitable chain to work with
        for( ; chainIt != featureChains.end(); chainIt ++) {
            base = chainIt->front()->getBaseObject ( /*silent =*/ true );
            if (!base || !base->isDerivedFrom ( PartDesign::Feature::getClassTypeId () ) ) {
                break;   // no base is ok
            } else {
                // The base feature is a PartDesign, it's a fork, try to reassign it to a body...
                base = PartDesign::Body::findBodyOf ( base );
                if ( base ) {
                    break;
                }
            }
        }

        if ( chainIt == featureChains.end() ) {
            // Shouldn't happen, may be only in case of some circular dependency?
            // TODO Some error message (2015-08-11, Fat-Zer)
            chainIt = featureChains.begin();
            base = chainIt->front()->getBaseObject ( /*silent =*/ true );
        }

        // Construct a Pretty Body name based on the Tip
        std::string bodyName = getUniqueObjectName (
                std::string ( chainIt->back()->getNameInDocument() ).append ( "Body" ).c_str () ) ;

        // Create a body for the chain
        doCommand ( Doc,"App.activeDocument().addObject('PartDesign::Body','%s')", bodyName.c_str () );
        doCommand ( Doc,"App.activeDocument().%s.addObject(App.ActiveDocument.%s)",
                actPart->getNameInDocument (), bodyName.c_str () );
        if (base) {
            doCommand ( Doc,"App.activeDocument().%s.BaseFeature = App.activeDocument().%s",
                bodyName.c_str (), base->getNameInDocument () );
        }

        // Fill the body with features
        for ( auto feature: *chainIt ) {
            if ( feature->isDerivedFrom ( PartDesign::ProfileBased::getClassTypeId() ) ) {
                // add the sketch and also reroute it if needed
                PartDesign::ProfileBased *sketchBased = static_cast<PartDesign::ProfileBased *> ( feature );
                Part::Part2DObject *sketch = sketchBased->getVerifiedSketch( /*silent =*/ true);
                if ( sketch ) {
                    doCommand ( Doc,"App.activeDocument().%s.addFeature(App.activeDocument().%s)",
                            bodyName.c_str (), sketch->getNameInDocument() );

                    if ( sketch->isDerivedFrom ( Sketcher::SketchObject::getClassTypeId() ) ) {
                        try {
                            PartDesignGui::fixSketchSupport ( static_cast<Sketcher::SketchObject *> ( sketch ) );
                        } catch (Base::Exception &) {
                            QMessageBox::critical(Gui::getMainWindow(),
                                    QObject::tr("Sketch plane cannot be migrated"),
                                    QObject::tr("Please edit '%1' and redefine it to use a Base or "
                                        "Datum plane as the sketch plane.").
                                    arg(QString::fromUtf8(sketch->Label.getValue()) ) );
                        }
                    } else {
                        // TODO Message that sketchbased is based not on a sketch (2015-08-11, Fat-Zer)
                    }
                }
            }
            doCommand ( Doc,"App.activeDocument().%s.addFeature(App.activeDocument().%s)",
                    bodyName.c_str (), feature->getNameInDocument() );

            PartDesignGui::relinkToBody ( feature );
        }

    }

    updateActive();
}

bool CmdPartDesignMigrate::isActive(void)
{
    return hasActiveDocument();
}

//===========================================================================
// PartDesign_MoveTip
//===========================================================================
DEF_STD_CMD_A(CmdPartDesignMoveTip);

CmdPartDesignMoveTip::CmdPartDesignMoveTip()
  : Command("PartDesign_MoveTip")
{
    sAppModule    = "PartDesign";
    sGroup        = QT_TR_NOOP("PartDesign");
    sMenuText     = QT_TR_NOOP("Set tip");
    sToolTipText  = QT_TR_NOOP("Move the tip of the body");
    sWhatsThis    = sToolTipText;
    sStatusTip    = sToolTipText;
    sPixmap       = "PartDesign_MoveTip";
}

void CmdPartDesignMoveTip::activated(int iMsg)
{
    std::vector<App::DocumentObject*> features = getSelection().getObjectsOfType(
            Part::Feature::getClassTypeId() );
    App::DocumentObject* selFeature;
    PartDesign::Body* body= nullptr;

    if ( features.size() == 1 ) {
        selFeature = features.front();
        if ( selFeature->getTypeId().isDerivedFrom ( PartDesign::Body::getClassTypeId() ) ) {
            body = static_cast<PartDesign::Body *> ( selFeature );
        } else {
            body = PartDesignGui::getBodyFor ( selFeature, /* messageIfNot =*/ false );
        }
    } else {
        selFeature = nullptr;
    }

    if (!selFeature) {
        QMessageBox::warning (0, QObject::tr( "Selection error" ),
                QObject::tr( "Select exactly one PartDesign feature or a body." ) );
        return;
    } else if (!body) {
        QMessageBox::warning (0, QObject::tr( "Selection error" ),
                QObject::tr( "Couldn't determin a body for the selected feature '%s'.", selFeature->Label.getValue() ) );
        return;
    } else if ( !selFeature->isDerivedFrom(PartDesign::Feature::getClassTypeId () ) &&
            selFeature != body && body->BaseFeature.getValue() != selFeature ) {
        QMessageBox::warning (0, QObject::tr( "Selection error" ),
                QObject::tr( "Only a solid feature can be the tip of a body." ) );
        return;
    }

    App::DocumentObject* oldTip = body->Tip.getValue();
    if (oldTip == selFeature) { // it's not generally an error, so print only a console message
        Base::Console().Message ("%s is already the tip of the body", selFeature->getNameInDocument () );
        return;
    }

    openCommand("Move tip to selected feature");

    if (selFeature == body) {
        doCommand(Doc,"App.activeDocument().%s.Tip = None", body->getNameInDocument());
    } else {
        doCommand(Doc,"App.activeDocument().%s.Tip = App.activeDocument().%s",body->getNameInDocument(),
                selFeature->getNameInDocument());

        // Adjust visibility to show only the Tip feature
        doCommand(Gui,"Gui.activeDocument().show(\"%s\")", selFeature->getNameInDocument());
    }

    // TOOD: Hide all datum features after the Tip feature? But the user might have already hidden some and wants to see
    // others, so we would have to remember their state somehow
    updateActive();
}

bool CmdPartDesignMoveTip::isActive(void)
{
    return hasActiveDocument();
}

//===========================================================================
// PartDesign_DuplicateSelection
//===========================================================================

DEF_STD_CMD_A(CmdPartDesignDuplicateSelection);

CmdPartDesignDuplicateSelection::CmdPartDesignDuplicateSelection()
  :Command("PartDesign_DuplicateSelection")
{
    sAppModule      = "PartDesign";
    sGroup          = QT_TR_NOOP("PartDesign");
    sMenuText       = QT_TR_NOOP("Duplicate selected object");
    sToolTipText    = QT_TR_NOOP("Duplicates the selected object and adds it to the active body");
    sWhatsThis      = sToolTipText;
    sStatusTip      = sToolTipText;
}

void CmdPartDesignDuplicateSelection::activated(int iMsg) {
    PartDesign::Body *pcActiveBody = PartDesignGui::getBody(/*messageIfNot = */false);

    std::vector<App::DocumentObject*> beforeFeatures = getDocument()->getObjects();

    openCommand("Duplicate a PartDesign object");
    doCommand(Doc,"FreeCADGui.runCommand('Std_DuplicateSelection')");

    if (pcActiveBody) {
        // Find the features that were added
        std::vector<App::DocumentObject*> afterFeatures = getDocument()->getObjects();
        std::vector<App::DocumentObject*> newFeatures;
        std::sort(beforeFeatures.begin(), beforeFeatures.end());
        std::sort(afterFeatures.begin(), afterFeatures.end());
        std::set_difference(afterFeatures.begin(), afterFeatures.end(), beforeFeatures.begin(), beforeFeatures.end(),
                            std::back_inserter(newFeatures));

        for (auto feature : newFeatures) {
            if (PartDesign::Body::isAllowed(feature)) {
                doCommand(Doc,"App.activeDocument().%s.addFeature(App.activeDocument().%s)",
                          pcActiveBody->getNameInDocument(), feature->getNameInDocument());
                doCommand(Gui,"Gui.activeDocument().hide(\"%s\")", feature->getNameInDocument());
            }
        }

        // Adjust visibility of features
        doCommand(Gui,"Gui.activeDocument().show(\"%s\")", newFeatures.back()->getNameInDocument());
    }

    updateActive();
}

bool CmdPartDesignDuplicateSelection::isActive(void)
{
    return hasActiveDocument();
}

//===========================================================================
// PartDesign_MoveFeature
//===========================================================================

DEF_STD_CMD_A(CmdPartDesignMoveFeature);

CmdPartDesignMoveFeature::CmdPartDesignMoveFeature()
  :Command("PartDesign_MoveFeature")
{
    sAppModule      = "PartDesign";
    sGroup          = QT_TR_NOOP("PartDesign");
    sMenuText       = QT_TR_NOOP("Move object to other body");
    sToolTipText    = QT_TR_NOOP("Moves the selected object to another body");
    sWhatsThis      = sToolTipText;
    sStatusTip      = sToolTipText;
    sPixmap         = "";
}

void CmdPartDesignMoveFeature::activated(int iMsg)
{
    std::vector<App::DocumentObject*> features = getSelection().getObjectsOfType(Part::Feature::getClassTypeId());
    if (features.empty()) return;

    // Check if all features are valid to move
    if (std::any_of(std::begin(features), std::end(features), [](App::DocumentObject* obj){return !PartDesignGui::isFeatureMovable(obj); }))
    {
        //show messagebox and cancel
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Features cannot be moved"),
            QObject::tr("Some of the selected features have dependencies in the source body"));
        return;
    }

    // Collect dependenies of the selected features
    std::vector<App::DocumentObject*> dependencies = PartDesignGui::collectMovableDependencies(features);
    if (!dependencies.empty())
        features.insert(std::end(features), std::begin(dependencies), std::end(dependencies));

    // Create a list of all bodies in this part
    std::vector<App::DocumentObject*> bodies = getDocument()->getObjectsOfType(Part::BodyBase::getClassTypeId());

    std::set<App::DocumentObject*> source_bodies;
    for (auto feat : features) {
        PartDesign::Body* source = PartDesign::Body::findBodyOf(feat);
        source_bodies.insert(static_cast<App::DocumentObject*>(source));
    }

    std::vector<App::DocumentObject*> target_bodies;
    for (auto body : bodies) {
        if (!source_bodies.count(body))
            target_bodies.push_back(body);
    }

    if (target_bodies.empty())
    {
        QMessageBox::warning(Gui::getMainWindow(), QObject::tr("Features cannot be moved"),
            QObject::tr("There are no other bodies to move to"));
        return;
    }

    // Ask user to select the target body (remove source bodies from list)
    bool ok;
    QStringList items;
    for (auto body : target_bodies) {
        items.push_back(QString::fromUtf8(body->Label.getValue()));
    }
    QString text = QInputDialog::getItem(Gui::getMainWindow(),
        qApp->translate("PartDesign_MoveFeature", "Select body"),
        qApp->translate("PartDesign_MoveFeature", "Select a body from the list"),
        items, 0, false, &ok);
    if (!ok) return;
    int index = items.indexOf(text);
    if (index < 0) return;

    PartDesign::Body* target = static_cast<PartDesign::Body*>(target_bodies[index]);

    openCommand("Move an object");

    for (auto feat: features) {
        // Find body of this feature
        Part::BodyBase* source = PartDesign::Body::findBodyOf(feat);
        bool featureWasTip = false;

        if (source == target) continue;

        // Remove from the source body if the feature belonged to a body
        if (source) {
            featureWasTip = (source->Tip.getValue() == feat);
            doCommand(Doc,"App.activeDocument().%s.removeFeature(App.activeDocument().%s)",
                      source->getNameInDocument(), (feat)->getNameInDocument());
        }

        App::DocumentObject* targetOldTip = target->Tip.getValue();

        // Add to target body (always at the Tip)
        doCommand(Doc,"App.activeDocument().%s.addFeature(App.activeDocument().%s)",
                      target->getNameInDocument(), (feat)->getNameInDocument());
        // Recompute to update the shape
        doCommand(Gui,"App.activeDocument().recompute()");

        // Adjust visibility of features
        // TODO: May be something can be done in view provider (2015-08-05, Fat-Zer)
        // If we removed the tip of the source body, make the new tip visible
        if ( featureWasTip ) {
            App::DocumentObject * sourceNewTip = source->Tip.getValue();
            if (sourceNewTip)
                doCommand(Gui,"Gui.activeDocument().show(\"%s\")", sourceNewTip->getNameInDocument());
        }

        // Hide old tip and show new tip (the moved feature) of the target body
        App::DocumentObject* targetNewTip = target->Tip.getValue();
        if ( targetOldTip != targetNewTip ) {
            if ( targetOldTip ) {
                doCommand(Gui,"Gui.activeDocument().hide(\"%s\")", targetOldTip->getNameInDocument());
            }
            if (targetNewTip) {
                doCommand(Gui,"Gui.activeDocument().show(\"%s\")", targetNewTip->getNameInDocument());
            }
        }

        // Fix sketch support
        if (feat->getTypeId().isDerivedFrom(Sketcher::SketchObject::getClassTypeId())) {
            Sketcher::SketchObject *sketch = static_cast<Sketcher::SketchObject*>(feat);
            try {
                PartDesignGui::fixSketchSupport(sketch);
            } catch (Base::Exception &) {
                QMessageBox::warning( Gui::getMainWindow(), QObject::tr("Sketch plane cannot be migrated"),
                        QObject::tr("Please edit '%1' and redefine it to use a Base or Datum plane as the sketch plane.").
                        arg( QString::fromLatin1( sketch->Label.getValue () ) ) );
            }
        }

        //relink origin for sketches and datums (coordinates)
        PartDesignGui::relinkToOrigin(feat, target);
    }

    updateActive();
}

bool CmdPartDesignMoveFeature::isActive(void)
{
    return hasActiveDocument () && !PartDesignGui::isLegacyWorkflow ( getDocument () );
}

DEF_STD_CMD_A(CmdPartDesignMoveFeatureInTree);

CmdPartDesignMoveFeatureInTree::CmdPartDesignMoveFeatureInTree()
  :Command("PartDesign_MoveFeatureInTree")
{
    sAppModule      = "PartDesign";
    sGroup          = QT_TR_NOOP("PartDesign");
    sMenuText       = QT_TR_NOOP("Move object after other object");
    sToolTipText    = QT_TR_NOOP("Moves the selected object and insert it after another object");
    sWhatsThis      = sToolTipText;
    sStatusTip      = sToolTipText;
    sPixmap         = "";
}

void CmdPartDesignMoveFeatureInTree::activated(int iMsg)
{
    std::vector<App::DocumentObject*> features = getSelection().getObjectsOfType(Part::Feature::getClassTypeId());
    if (features.empty()) return;

    PartDesign::Body *body = PartDesignGui::getBodyFor ( features.front(), false );
    App::DocumentObject * bodyBase = nullptr;
    // sanity check
    bool allFeaturesFromSameBody = true;

    if ( body ) {
        bodyBase= body->BaseFeature.getValue();
        for ( auto feat: features ) {
            if ( !body->hasFeature ( feat ) ) {
                allFeaturesFromSameBody = false;
                break;
            }
            if ( bodyBase== feat) {
                QMessageBox::warning (0, QObject::tr( "Selection error" ),
                        QObject::tr( "Impossible to move the base feature of a body." ) );
                return;
            }
        }
    }
    if (!body || ! allFeaturesFromSameBody) {
        QMessageBox::warning (0, QObject::tr( "Selection error" ),
                QObject::tr( "Select one or more features from the same body." ) );
        return;
    }

    // Create a list of all features in this body
    const std::vector<App::DocumentObject*> & model = body->Model.getValues();

    // Ask user to select the target feature
    bool ok;
    QStringList items;
    if ( bodyBase ) {
        items.push_back( QString::fromUtf8 ( bodyBase->Label.getValue () ) );
    } else {
        items.push_back( QObject::tr( "Beginning of the body" ) );
    }
    for ( auto feat: model ) {
        items.push_back( QString::fromUtf8 ( feat->Label.getValue() ) );
    }

    QString text = QInputDialog::getItem(Gui::getMainWindow(),
        qApp->translate("PartDesign_MoveFeatureInTree", "Select feature"),
        qApp->translate("PartDesign_MoveFeatureInTree", "Select a feature from the list"),
        items, 0, false, &ok);
    if (!ok) return;
    int index = items.indexOf(text);
    // first object is the beginning of the body
    App::DocumentObject* target = index != 0 ? model[index-1] : nullptr;

    openCommand("Move an object inside tree");

    for ( auto feat: features ) {
        if ( feat == target ) continue;

        std::string targetStr;
        if (target) {
            targetStr.append("App.activeDocument().").append(target->getNameInDocument());
        } else {
            targetStr = "None";
        }

        // Remove and re-insert the feature to/from the Body
        // TODO if tip was moved the new position of tip is quite undetermined (2015-08-07, Fat-Zer)
        // TODO warn the user if we are moving an object to some place before the object's link (2015-08-07, Fat-Zer)
        doCommand ( Doc,"App.activeDocument().%s.removeFeature(App.activeDocument().%s)",
                body->getNameInDocument(), feat->getNameInDocument() );
        doCommand ( Doc, "App.activeDocument().%s.insertFeature(App.activeDocument().%s, %s, True)",
                body->getNameInDocument(), feat->getNameInDocument(), targetStr.c_str () );
    }

    updateActive();
}

bool CmdPartDesignMoveFeatureInTree::isActive(void)
{
    return hasActiveDocument () && !PartDesignGui::isLegacyWorkflow ( getDocument () );
}


//===========================================================================
// Initialization
//===========================================================================

void CreatePartDesignBodyCommands(void)
{
    Gui::CommandManager &rcCmdMgr = Gui::Application::Instance->commandManager();

    rcCmdMgr.addCommand(new CmdPartDesignPart());
    rcCmdMgr.addCommand(new CmdPartDesignBody());
    rcCmdMgr.addCommand(new CmdPartDesignMigrate());
    rcCmdMgr.addCommand(new CmdPartDesignMoveTip());

    rcCmdMgr.addCommand(new CmdPartDesignDuplicateSelection());
    rcCmdMgr.addCommand(new CmdPartDesignMoveFeature());
    rcCmdMgr.addCommand(new CmdPartDesignMoveFeatureInTree());
}
