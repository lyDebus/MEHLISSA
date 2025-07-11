/* -*-  Mode: C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Universität zu Lübeck [GEYER]
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Author: Regine Wendt <regine.wendt@uni-luebeck.de>
 */

#include "Bloodvessel.h"

using namespace std;
namespace ns3 {

static Time now, old;

TypeId Bloodvessel::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::Bloodvessel")
                            .SetParent<Object>()
                            .AddConstructor<Bloodvessel>();
    return tid;
}

Bloodvessel::Bloodvessel() {
    m_deltaT = 1;
    m_stepsPerSec = 1 / m_deltaT;
    m_secStepCounter = 0;
    initStreams();
    m_changeStreamSet = true;
    m_basevelocity = 0;
    m_currentStream = 0;
    m_transitionto1 = 1;
    m_transitionto2 = 0;
    m_hasActiveFingerprintMessage = false;
    m_isGatewayVessel = false;
    injection.m_injectionTime = -1;
    injection.m_injectionVessel = -1;
    injection.m_injectionNumber = -1;
}

Bloodvessel::~Bloodvessel() {
    /*  for (int i = 0; i < m_numberOfStreams; i++)
      {
        m_nanobots[i].erase (m_nanobots[i].begin (), m_nanobots[i].end ());
      }
      */
}
void Bloodvessel::SetPrinter(Ptr<PrintNanobots> printer) {
    this->printer = printer;
}

void Bloodvessel::Start() {
    m_start = true;
    Simulator::Schedule(Seconds(0.0), &Step, Ptr<Bloodvessel>(this));
    old = Simulator::Now();
}

void Bloodvessel::Step(Ptr<Bloodvessel> bloodvessel) {
    if (bloodvessel->GetbloodvesselID() == 1) {
        now = Simulator::Now();
        long inSeconds = now.GetMicroSeconds() - old.GetMicroSeconds();
        cout << inSeconds / 1000000 << "s" << endl;
    }
    bloodvessel->CountStepsAndAgeCells();
    bloodvessel->TranslateNanobots();
    bloodvessel->PerformCellMitosis();

    // to initiate a second particle release, change variable
    // secondParticleRelease from false to true
    bool secondParticleRelease = 1;
    if (secondParticleRelease) {
        now = Simulator::Now();
        long inSeconds = now.GetMicroSeconds() - old.GetMicroSeconds();
        if (bloodvessel->GetbloodvesselID() == 36 &&
            (inSeconds / 1000000) == 600)
            bloodvessel->ReleaseParticles();
    }

    if (bloodvessel->injection.m_injectionVessel > 0) {
        now = Simulator::Now();
        long inSeconds = now.GetMicroSeconds() - old.GetMicroSeconds();
        if (bloodvessel->injection.m_injectionTime == (inSeconds / 1000000)) {
            cout << "Injecting CAR-T cells now" << endl;
            cout << "Vessel ID: " << bloodvessel->GetbloodvesselID() << endl;
            bloodvessel->PerformInjection();
            bloodvessel->injection.m_injectionVessel = -1;
        }
    }
}

Vector Bloodvessel::SetPosition(Vector nbv, double distance, double angle,
                                int bloodvesselType, double startPosZ) {
    // Check vessel direction and move according to distance.
    // right
    if (angle == 0.00 && bloodvesselType != ORGAN) {
        nbv.x += distance;
    }
    // left
    else if (angle == -180.00 || angle == 180.00) {
        nbv.x -= distance;
    }
    // down
    else if (angle == -90.00) {
        nbv.y -= distance;
    }
    // up
    else if (angle == 90.00) {
        nbv.y += distance;
    }
    // back
    else if (angle == 0.00 && bloodvesselType == ORGAN && startPosZ == 2) {
        nbv.z -= distance;
    }
    // front
    else if (angle == 0.00 && bloodvesselType == ORGAN && startPosZ == -2) {
        nbv.z += distance;
    }
    // right up
    else if ((0.00 < angle && angle < 90.00) ||
             (-90.00 < angle && angle < 0.00) ||
             (90.00 < angle && angle < 180.00) ||
             (-180.00 < angle && angle < -90.00)) {
        nbv.x += distance * (cos(fmod((angle), 360) * M_PI / 180));
        nbv.y += distance * (sin(fmod((angle), 360) * M_PI / 180));
    }
    return nbv;
}

void Bloodvessel::TranslateNanobots() {
    if (m_start) {
        Simulator::Schedule(Seconds(m_deltaT), &Bloodvessel::Step,
                            Ptr<Bloodvessel>(this));
        m_start = false;
        return;
    }
    if (this->IsEmpty()) {
        Simulator::Schedule(Seconds(m_deltaT), &Bloodvessel::Step,
                            Ptr<Bloodvessel>(this));
        return;
    }
    static int loop = 1;
    if (loop == 2)
        loop = 0;
    // Change streams only in organs
    if (loop == 0 && m_changeStreamSet == true &&
        this->GetBloodvesselType() == ORGAN) {
        ChangeStream();
    }
    // Translate their position every timestep
    TranslatePosition(m_deltaT);
    loop++;
    Simulator::Schedule(Seconds(m_deltaT), &Bloodvessel::Step,
                        Ptr<Bloodvessel>(this));
}

bool Bloodvessel::moveNanobot(Ptr<Nanobot> nb, int i, int randVelocityOffset,
                              bool direction, double dt) {
    double distance = 0.0;
    double velocity = m_bloodstreams[i]->GetVelocity();
    if (nb->GetDelay() >= 0)
        velocity = velocity * (nb->GetDelay());

    if (direction)
        distance = (velocity - ((velocity / 100) * randVelocityOffset)) * dt;
    else
        distance = (velocity + ((velocity / 100) * randVelocityOffset)) * dt;
    // Check vessel direction and move accordingly.
    Vector nbv =
        SetPosition(nb->GetPosition(), distance, GetBloodvesselAngle(),
                    GetBloodvesselType(), m_startPositionBloodvessel.z);
    nb->SetPosition(nbv);
    nb->SetTimeStep();
    double nbx = nb->GetPosition().x - m_startPositionBloodvessel.x;
    double nby = nb->GetPosition().y - m_startPositionBloodvessel.y;
    double length = sqrt(nbx * nbx + nby * nby);
    // check if position exceeds bloodvessel
    return (length > m_bloodvesselLength || (nbv.z < -2 && m_angle == 0) ||
            (nbv.z > 2 && m_angle == 0));
}

void Bloodvessel::TranslatePosition(double dt) {
    list<Ptr<Nanobot>> print;
    list<Ptr<Nanobot>> reachedEnd;

    Ptr<UniformRandomVariable> randomVelocityOffset =
        Randomizer::GetNewRandomStream(0, 11);

    int numCarTCells = 0;
    int numCancerCells = 0;

    // perform interaction between CarTCells and Cancer Cells
    for (int i = 0; i < m_numberOfStreams; i++) {
        for (uint j = 0; j < m_bloodstreams[i]->CountNanobots(); j++) {
            Ptr<Nanobot> nb = m_bloodstreams[i]->GetNanobot(j);
            if (nb->m_type == CarTCellType) {
                // cout << "Found CarTCell" << endl;
                Ptr<Nanobot> nb = m_bloodstreams[i]->GetNanobot(j);
                if (!nb->IsAlive()) {
                    m_bloodstreams[i]->RemoveNanobot(nb);
                    j =-1;
                    continue;
                }
                Nanobot* nb_p = PeekPointer(nb);
                CarTCell* ctc = dynamic_cast<CarTCell*>(nb_p);
                if (ctc == NULL)
                    continue;
                for (int k = 0; k < m_numberOfStreams; k++) {
                    for (uint l = 0; l < m_bloodstreams[k]->CountNanobots();
                         l++) {
                        Ptr<Nanobot> nb2 = m_bloodstreams[k]->GetNanobot(l);
                        ctc->AddPossibleMitosis(nb2->m_type);
                        switch (nb2->m_type) {
                        case CancerCellType: {
                            // cout << "Found CancerCell" << endl;
                            Nanobot* nb2_p = PeekPointer(nb2);
                            CancerCell* cc = dynamic_cast<CancerCell*>(nb2_p);
                            if (cc == NULL)
                                continue;
                            double dist = CalcDistance(nb, nb2);
                            if (dist <= nb2->GetDetectionRadius()) {
                                if (ctc->KillCancerCell() == true) {
                                    // cout << "Killing CancerCell" << endl;
                                    cc->GetsDetected();
                                    m_bloodstreams[k]->RemoveNanobot(nb2);
                                    l -= 1;
                                }
                            }
                            break;
                        }
                        case TCellType: {
                            // cout << "Found TCell" << endl;
                            Nanobot* nb2_p = PeekPointer(nb2);
                            TCell* tc = dynamic_cast<TCell*>(nb2_p);
                            if (tc == NULL)
                                continue;
                            double dist = CalcDistance(nb, nb2);
                            if (dist <= tc->GetDetectionRadius()) {
                                if (ctc->KillTCell() == true) {
                                    // cout << "Killing TCell" << endl;
                                    tc->GetsDetected();
                                    m_bloodstreams[k]->RemoveNanobot(nb2);
                                    l -= 1;
                                }
                            }
                            break;
                        }
                        case CarTCellType: {
                            // cout << "Found other CarTCell" << endl;
                            Nanobot* nb2_p = PeekPointer(nb2);
                            CarTCell* ctc2 = dynamic_cast<CarTCell*>(nb2_p);
                            if (ctc2 == NULL)
                                continue;
                            double dist = CalcDistance(nb, nb2);
                            if (dist <= 0) {
                                if (ctc->KillCarTCell() == true) {
                                    // cout << "Killing other CarTCell" << endl;
                                    m_bloodstreams[k]->RemoveNanobot(nb2);
                                    l -= 1;
                                }
                            }
                            break;
                        }
                        default:
                            break;
                        }
                    }
                }
            }
        }
    }

    // for every stream of the vessel
    for (int i = 0; i < m_numberOfStreams; i++) {
        // for every nanobot of the stream
        for (uint j = 0; j < m_bloodstreams[i]->CountNanobots(); j++) {
            Ptr<Nanobot> nb = m_bloodstreams[i]->GetNanobot(j);
            Nanobot* nb_p = PeekPointer(nb);
            CarTCell* ctc = dynamic_cast<CarTCell*>(nb_p);
            if (nb->m_type == CarTCellType) {
                CarTCell* ctc = dynamic_cast<CarTCell*>(nb_p);
                if (ctc != NULL && ctc->IsActive())
                    numCarTCells++;
            }
            if (nb->m_type == CancerCellType)
                numCancerCells++;
            CancerCell* cc = dynamic_cast<CancerCell*>(nb_p);
            if (cc != NULL && cc->MustBeDeleted()) {
                // cout << "Removed Cancer Cell" << endl;
                m_bloodstreams[i]->RemoveNanobot(nb);
                j -= 1;
                continue;
            }
            // move only nanobots that have not already been translated by
            // another vessel
            if (nb->GetTimeStep() < Simulator::Now()) {
                // has nanobot reached end after moving
                if (moveNanobot(nb, i, randomVelocityOffset->GetValue(),
                                Randomizer::GetRandomBoolean(), dt))
                    reachedEnd.push_back(nb);
                else
                    print.push_back(nb);
                // PrintNanobot (nb, this);
            } // if timestep not from old
        } // inner for
        // Push Nanobots that reached the end of the bloodvessel to next vessel.
        if (reachedEnd.size() > 0)
            TransposeNanobots(reachedEnd, i);
        reachedEnd.clear();
    } // outer for
    // disable printing printer->PrintSomeNanobots(print, this->GetbloodvesselID());
    // disable printing if (m_isGatewayVessel == true || m_bloodvesselID == 1)
        // disable printing printer->PrintGateway(m_bloodvesselID, numCancerCells, numCarTCells);
} // Function

void Bloodvessel::ChangeStream() {
    if (m_numberOfStreams > 1) {
        // set half of the nanobots randomly to change
        for (int i = 0; i < m_numberOfStreams; i++) {
            for (uint j = 0; j < m_bloodstreams[i]->CountNanobots(); j++) {
                if (Randomizer::GetRandomBoolean())
                    m_bloodstreams[i]->GetNanobot(j)->SetShouldChange(true);
            }
        }
        // after all nanobots that should change are flagged, do change
        for (int i = 0; i < m_numberOfStreams; i++) {
            int direction = Randomizer::GetRandomBoolean() == true ? -1 : 1;
            if (i == 0) // Special Case 1: outer lane left -> go to middle
                direction = 1;
            else if (i + 1 >=
                     m_numberOfStreams) // Special Case 2: outer lane right
                                        // -> go to middle
                direction = -1;

            // Move randomly left or right
            DoChangeStreamIfPossible(i, i + direction);
        }
    }
}

void Bloodvessel::DoChangeStreamIfPossible(int curStream, int desStream) {
    list<Ptr<Nanobot>> canChange;
    canChange.clear();
    for (uint j = 0; j < m_bloodstreams[curStream]->CountNanobots(); j++) {
        if (m_bloodstreams[curStream]->GetNanobot(j)->GetShouldChange()) {
            // set should change back to false
            m_bloodstreams[curStream]->GetNanobot(j)->SetShouldChange(false);
            m_bloodstreams[desStream]->AddNanobot(
                m_bloodstreams[curStream]->RemoveNanobot(j));
        }
    }
    // Sort all Nanobots by ID
    m_bloodstreams[desStream]->SortStream();
}

bool Bloodvessel::transposeNanobot(Ptr<Nanobot> botToTranspose,
                                   Ptr<Bloodvessel> thisBloodvessel,
                                   Ptr<Bloodvessel> nextBloodvessel,
                                   int stream) {
    Vector stopPositionOfVessel = thisBloodvessel->GetStopPositionBloodvessel();
    Vector nanobotPosition = botToTranspose->GetPosition();
    double distance = sqrt(pow(nanobotPosition.x - stopPositionOfVessel.x, 2) +
                           pow(nanobotPosition.y - stopPositionOfVessel.y, 2) +
                           pow(nanobotPosition.z - stopPositionOfVessel.z, 2));
    distance = distance /
               thisBloodvessel->m_bloodstreams[stream]->GetVelocity() *
               nextBloodvessel->m_bloodstreams[stream]->GetVelocity();
    botToTranspose->SetPosition(nextBloodvessel->GetStartPositionBloodvessel());
    Vector rmp = SetPosition(botToTranspose->GetPosition(), distance,
                             nextBloodvessel->GetBloodvesselAngle(),
                             nextBloodvessel->GetBloodvesselType(),
                             thisBloodvessel->GetStopPositionBloodvessel().z);
    botToTranspose->SetPosition(rmp);
    double nbx = botToTranspose->GetPosition().x -
                 nextBloodvessel->GetStartPositionBloodvessel().x;
    double nby = botToTranspose->GetPosition().y -
                 nextBloodvessel->GetStartPositionBloodvessel().y;
    double length = sqrt(nbx * nbx + nby * nby);
    // check if position exceeds bloodvessel
    return length > nextBloodvessel->GetbloodvesselLength() || rmp.z < -2 ||
           rmp.z > 2;
}

void Bloodvessel::TransposeNanobots(list<Ptr<Nanobot>> reachedEnd, int stream) {
    list<Ptr<Nanobot>> print1;
    list<Ptr<Nanobot>> print2;
    list<Ptr<Nanobot>> reachedEndAgain;

    Ptr<UniformRandomVariable> rv = Randomizer::GetNewRandomStream(0, 10000);
    int onetwo;
    for (const Ptr<Nanobot> &botToTranspose : reachedEnd) {
        onetwo = floor(rv->GetValue()); // 0-9999
        // Remove Nanobot from actual bloodstream
        m_bloodstreams[stream]->RemoveNanobot(botToTranspose);

        // to v2
        if (m_nextBloodvessel2 != 0 && (onetwo >= m_transitionto1 * 10000)) {
            // fits next vessel?
            if (transposeNanobot(botToTranspose, this, m_nextBloodvessel2,
                                 stream)) {
                reachedEndAgain.push_back(botToTranspose);
                m_nextBloodvessel2->TransposeNanobots(reachedEndAgain, stream);
                reachedEndAgain.clear();
            } else {
                m_nextBloodvessel2->m_bloodstreams[stream]->AddNanobot(
                    botToTranspose);
                print2.push_back(botToTranspose);
            }
        }
        // to v1
        else if (m_nextBloodvessel2 == 0 || onetwo < m_transitionto1 * 10000) {
            // fits next vessel?
            if (transposeNanobot(botToTranspose, this, m_nextBloodvessel1,
                                 stream)) {
                reachedEndAgain.push_back(botToTranspose);
                m_nextBloodvessel1->TransposeNanobots(reachedEndAgain, stream);
                reachedEndAgain.clear();
            } else {
                m_nextBloodvessel1->m_bloodstreams[stream]->AddNanobot(
                    botToTranspose);
                print1.push_back(botToTranspose);
            }
        }
    } // for

    // disable printing printer->PrintSomeNanobots(print1, m_nextBloodvessel1->GetbloodvesselID());
    // All Nanobots that freshly entered a vessel in this step are now in print1
    // (and print2). Now we check if these Nanobots are Nanolocators, that
    // should release molecules because they are in their target organ.
    if (m_nextBloodvessel1->GetFingerprintFormationTime() > 0)
        m_nextBloodvessel1->CheckRelease(print1);
    // Now we check if these Nanobots are Nanocollectors and if there is an
    // active Message molecule for them in the organ that can get collected
    if (m_nextBloodvessel1->isActive())
        m_nextBloodvessel1->CheckCollect(print1);
    // Now we check if these Nanobots are Nanobots and Nanoparticles mixed and
    // if the nanobots can detect nanoparticles
    m_nextBloodvessel1->CheckDetect(print1);

    // check for the second branch if present
    if (m_nextBloodvessel2) {
        // disable printing printer->PrintSomeNanobots(print2,
        // disable printing                            m_nextBloodvessel2->GetbloodvesselID());
        if (m_nextBloodvessel2->GetFingerprintFormationTime() > 0)
            m_nextBloodvessel2->CheckRelease(print2);
        if (m_nextBloodvessel2->isActive())
            m_nextBloodvessel2->CheckCollect(print2);
        // Now we check if these Nanobots are Nanobots and Nanoparticles mixed
        // and if the nanobots can detect nanoparticles
        m_nextBloodvessel1->CheckDetect(print1);
    }
}

// HELPER
void Bloodvessel::PrintNanobotsOfVessel() {
    // disable printing for (uint j = 0; j < m_bloodstreams.size(); j++) {
        // disable printing for (uint i = 0; i < m_bloodstreams[j]->CountNanobots(); i++)
            // disable printing printer->PrintNanobot(m_bloodstreams[j]->GetNanobot(i),
                                  // disable printing GetbloodvesselID());
    // disable printing }
}

void Bloodvessel::initStreams() {
    int i;
    Ptr<Bloodstream> stream;
    for (i = 0; i < stream_definition_size; i++) {
        stream = CreateObject<Bloodstream>();
        stream->initBloodstream(m_bloodvesselID, i, stream_definition[i][0],
                                stream_definition[i][1] / 10.0,
                                stream_definition[i][2] / 10.0,
                                GetBloodvesselAngle());
        m_bloodstreams.push_back(stream);
    }
    m_numberOfStreams = stream_definition_size;
}

double Bloodvessel::CalcLength() {
    if (GetBloodvesselType() == ORGAN) {
        return 4;
    } else {
        Vector m_start = GetStartPositionBloodvessel();
        Vector m_end = GetStopPositionBloodvessel();
        double l =
            sqrt(pow(m_end.x - m_start.x, 2) + pow(m_end.y - m_start.y, 2));
        return l;
    }
}

double Bloodvessel::CalcDistance(Ptr<Nanobot> n_1, Ptr<Nanobot> n_2) {
    Vector v_1 = n_1->GetPosition();
    Vector v_2 = n_2->GetPosition();
    double l = sqrt(pow(v_1.x - v_2.x, 2) + pow(v_1.y - v_2.y, 2));
    return l;
}

double Bloodvessel::CalcAngle() {
    Vector m_start = GetStartPositionBloodvessel();
    Vector m_end = GetStopPositionBloodvessel();
    double x = m_end.x - m_start.x;
    double y = m_end.y - m_start.y;
    return atan2(y, x) * 180 / M_PI;
}

void Bloodvessel::CountStepsAndAgeCells() {
    m_secStepCounter++;
    if (m_secStepCounter >= m_stepsPerSec) {
        m_secStepCounter = 0;
        int secCount = 1;
        if (m_stepsPerSec < 0)
            secCount = m_deltaT;
        for (int i = 0; i < m_numberOfStreams; i++) {
            for (uint j = 0; j < m_bloodstreams[i]->CountNanobots(); j++) {
                Ptr<Nanobot> nb = m_bloodstreams[i]->GetNanobot(j);
                if (nb->CanAge() && !nb->Age(secCount)) {
                    m_bloodstreams[i]->RemoveNanobot(nb);
                    j -= 1;
                }
            }
        }
    }
}

void Bloodvessel::PerformCellMitosis() {
    for (int i = 0; i < m_numberOfStreams; i++) {
        for (uint j = 0; j < m_bloodstreams[i]->CountNanobots(); j++) {
            Ptr<Nanobot> nb = m_bloodstreams[i]->GetNanobot(j);
            if (nb->WillPerformMitosis()) {
                switch (nb->m_type) {
                case CarTCellType: {
                    Vector m_coordinates = this->GetStartPositionBloodvessel();
                    //Vector m_coordinates = nb->GetPosition();
                    Ptr<CarTCell> cell = CreateObject<CarTCell>();
                    cell->SetNanobotID(IDCounter::GetNextNanobotID());
                    cell->SetShouldChange(false);
                    cell->SetPosition(Vector(m_coordinates.x, 
                                             m_coordinates.y, 
                                             m_coordinates.z));
                    this->AddNanobotToStream(i, cell);
                    break;
                }
                case CancerCellType: {
                    Vector m_coordinates = this->GetStartPositionBloodvessel();
                    //Vector m_coordinates = nb->GetPosition();
                    Ptr<CancerCell> cell = CreateObject<CancerCell>();
                    cell->SetNanobotID(IDCounter::GetNextNanobotID());
                    cell->SetShouldChange(false);
                    cell->SetPosition(Vector(m_coordinates.x, 
                                             m_coordinates.y, 
                                             m_coordinates.z));
                    this->AddNanobotToStream(i, cell);
                    break;
                }
                default:
                    break;
                }
                nb->ResetMitosis();
            }
        }
    }
    
}

void Bloodvessel::InitBloodstreamLengthAngleAndVelocity(double velocity) {
    int i;
    double length = CalcLength();
    m_bloodvesselLength = length < 0 ? 10000 : length;
    m_angle = CalcAngle();

    if (velocity >= 0) {
        m_basevelocity = velocity;
        int maxLength = 0;
        // calulate the width per stream
        for (i = 0; i < m_numberOfStreams; i++) {
            if (maxLength < stream_definition[i][1])
                maxLength = stream_definition[i][1];
            if (maxLength < stream_definition[i][2])
                maxLength = stream_definition[i][2];
        }
        double offset = m_vesselWidth / 2.0 / maxLength;
        // change velocity for the hearts
        if (m_bloodvesselID == 2 || m_bloodvesselID == 58)
            m_basevelocity = 5; // duration of one cardiac cycle 0.8 seconds and
                                // 4 cm distance.
        // Set velocity, angle and position offset
        for (i = 0; i < m_numberOfStreams; i++) {
            m_bloodstreams[i]->SetVelocity(m_basevelocity);
            m_bloodstreams[i]->SetAngle(m_angle,
                                        stream_definition[i][1] * offset,
                                        stream_definition[i][2] * offset);
        }
    }
}

void Bloodvessel::CheckRelease(list<Ptr<Nanobot>> nbToCheck) {
    for (const Ptr<Nanobot> &bot : nbToCheck) {
        if (bot->HasFingerprintLoaded()) {
            if (bot->GetTargetOrgan() == m_bloodvesselID) {
                // std::cout << "Nanolocator: " << bot->GetNanobotID() << " has
                // Target " << bot->GetTargetOrgan() << std::endl;
                Simulator::Schedule(Seconds(m_fingerprintFormationTime),
                                    &Bloodvessel::TimerCallback, this);
                // When one Nanolocator reached the vessel it is assumed that
                // others will follow and the signal is strong enough. So the
                // vessel doesn't look for more nanolocators
                // m_fingerprintFormationflag = false;
                bot->releaseFingerprintTiles();
            }
        }
    }
}

void Bloodvessel::CheckCollect(list<Ptr<Nanobot>> nbToCheck) {
    for (const Ptr<Nanobot> &bot : nbToCheck) {
        // Bot is nanocollector
        if (bot->m_type == NanocollectorType) {
            if (bot->GetTargetOrgan() == m_bloodvesselID)
                // std::cout << "Nanocollector: " << bot->GetNanobotID() << "
                // collects message " << bot->GetTargetOrgan() << std::endl;
                bot->collectMessage();
        }
    }
}

void Bloodvessel::CheckDetect(list<Ptr<Nanobot>> nbToCheck) {
    for (const Ptr<Nanobot> &particle : nbToCheck) {
        // Bot is nanoparticle
        if (particle->m_type == NanoparticleType) {
            double x = particle->GetPosition().x;
            double y = particle->GetPosition().y;
            double z = particle->GetPosition().z;
            double detectionRadius = particle->GetDetectionRadius();

            for (const Ptr<Nanobot> &bot : nbToCheck) {
                // is Nanobot
                if (bot->m_type == NanobotType) {
                    double bx = bot->GetPosition().x;
                    double by = bot->GetPosition().y;
                    double bz = bot->GetPosition().z;

                    // calculate distance
                    double distance =
                        sqrt(pow(x - bx, 2) + pow(y - by, 2) + pow(z - bz, 2));
                    // is in radius of Detection
                    if (distance < detectionRadius)
                        particle->GetsDetected();
                }
            }
        }
    }
}

// GETTER AND SETTER

bool Bloodvessel::IsEmpty() {
    bool empty = true;
    for (int i = 0; i < m_numberOfStreams; i++)
        empty = empty && m_bloodstreams[i]->IsEmpty();
    return empty;
}

int Bloodvessel::GetbloodvesselID() { return m_bloodvesselID; }

void Bloodvessel::SetBloodvesselID(int b_id) { m_bloodvesselID = b_id; }

double Bloodvessel::GetBloodvesselAngle() { return m_angle; }

int Bloodvessel::GetNumberOfStreams() { return m_numberOfStreams; }

Ptr<Bloodstream> Bloodvessel::GetStream(int id) { return m_bloodstreams[id]; }

double Bloodvessel::GetbloodvesselLength() { return m_bloodvesselLength; }

void Bloodvessel::SetVesselWidth(double value) { m_vesselWidth = value; }

void Bloodvessel::AddNanobotToStream(unsigned int streamID, Ptr<Nanobot> bot) {
    m_bloodstreams[streamID]->AddNanobot(bot);
}

BloodvesselType Bloodvessel::GetBloodvesselType() { return m_bloodvesselType; }

void Bloodvessel::SetBloodvesselType(BloodvesselType value) {
    m_bloodvesselType = value;
}

void Bloodvessel::SetNextBloodvessel1(Ptr<Bloodvessel> value) {
    m_nextBloodvessel1 = value;
}

void Bloodvessel::SetNextBloodvessel2(Ptr<Bloodvessel> value) {
    m_nextBloodvessel2 = value;
}

void Bloodvessel::SetTransition1(double value) { m_transitionto1 = value; }

void Bloodvessel::SetTransition2(double value) { m_transitionto2 = value; }
void Bloodvessel::SetFingerprintFormationTime(double value) {
    m_fingerprintFormationTime = value;
}

double Bloodvessel::GetFingerprintFormationTime() {
    return m_fingerprintFormationTime;
}

Vector Bloodvessel::GetStartPositionBloodvessel() {
    return m_startPositionBloodvessel;
}

void Bloodvessel::SetStartPositionBloodvessel(Vector value) {
    m_startPositionBloodvessel = value;
}

Vector Bloodvessel::GetStopPositionBloodvessel() {
    return m_stopPositionBloodvessel;
}

void Bloodvessel::SetStopPositionBloodvessel(Vector value) {
    m_stopPositionBloodvessel = value;
}

bool Bloodvessel::IsGatewayVessel() { return m_isGatewayVessel; }

void Bloodvessel::SetIsGatewayVessel(bool value) { m_isGatewayVessel = value; }

void Bloodvessel::TimerCallback() {
    m_hasActiveFingerprintMessage = true;
    std::cout << "Timer expired! Fingerprint message received "
              << m_hasActiveFingerprintMessage
              << " in organ: " << m_bloodvesselID << std::endl;
}
bool Bloodvessel::isActive() { return m_hasActiveFingerprintMessage; }

void Bloodvessel::ReleaseParticles() {
    // release more particles
    Ptr<UniformRandomVariable> distribute_randomly =
        Randomizer::GetNewRandomStream(0, this->GetNumberOfStreams());
    // release particles from the liver Organ 36
    for (int i = 1; i <= 100; ++i) {
        // Ptr<Bloodvessel> liver = m_bloodvessels[36];
        Vector m_coordinateLiver = this->GetStartPositionBloodvessel();
        Ptr<Nanobot> temp_np = CreateObject<Nanoparticle>();
        temp_np->SetNanobotID(IDCounter::GetNextNanobotID());
        // Get random stream number.
        int dr = floor(distribute_randomly->GetValue());
        temp_np->SetShouldChange(false);
        temp_np->SetPosition(Vector(m_coordinateLiver.x, m_coordinateLiver.y,
                                    m_coordinateLiver.z));
        // Set Speed and Detection Radius of particles
        temp_np->SetDelay(2.32);
        temp_np->SetDetectionRadius(0.2);
        // Set position with random stream dr.
        this->AddNanobotToStream(dr, temp_np);
    }
}

void Bloodvessel::PerformInjection() {
    Ptr<UniformRandomVariable> distribute_randomly =
        Randomizer::GetNewRandomStream(0, this->GetNumberOfStreams());
    for (int i = 1; i <= injection.m_injectionNumber; ++i) {
        Vector m_coordinates = this->GetStartPositionBloodvessel();
        Ptr<CarTCell> cell = CreateObject<CarTCell>();
        cell->SetNanobotID(IDCounter::GetNextNanobotID());
        cell->SetShouldChange(false);
        cell->SetPosition(
            Vector(m_coordinates.x, m_coordinates.y, m_coordinates.z));
        int dr = floor(distribute_randomly->GetValue());
        this->AddNanobotToStream(dr, cell);
    }
}

void Bloodvessel::AddCarTCellInjection(int injectionTime, int injectionVessel,
                                       int numberOfCarTCells) {
    injection.m_injectionTime = injectionTime;
    injection.m_injectionVessel = injectionVessel;
    injection.m_injectionNumber = numberOfCarTCells;
}

int Bloodvessel::CountCancerCells() {
    int cancerCells = 0;
    for (int i = 0; i < m_numberOfStreams; i++)
        cancerCells += m_bloodstreams[i]->CountCancerCells();
    return cancerCells;
}

int Bloodvessel::CountCarTCells() {
    int carTCells = 0;
    for (int i = 0; i < m_numberOfStreams; i++)
        carTCells += m_bloodstreams[i]->CountCarTCells();
    return carTCells;
}

} // namespace ns3
