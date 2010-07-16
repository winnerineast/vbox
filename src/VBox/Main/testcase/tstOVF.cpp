/* $Id$ */
/** @file
 *
 * tstOVF - testcases for OVF import and export
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/VirtualBox.h>

#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/string.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>

#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/param.h>

#include <list>

using namespace com;

// main
///////////////////////////////////////////////////////////////////////////////

/**
 * Quick hack exception structure.
 *
 */
struct MyError
{
    MyError(HRESULT rc,
            const char *pcsz,
            IProgress *pProgress = NULL)
        : m_rc(rc)
    {
        m_str = "ERROR: ";
        m_str += pcsz;

        if (pProgress)
        {
            com::ProgressErrorInfo info(pProgress);
            com::GluePrintErrorInfo(info);
        }
        else if (rc)
        {
            com::ErrorInfo info;
            if (!info.isFullAvailable() && !info.isBasicAvailable())
                com::GluePrintRCMessage(rc);
            else
                com::GluePrintErrorInfo(info);
        }
    }

    Utf8Str m_str;
    HRESULT m_rc;
};

/**
 * Imports the given OVF file, with all bells and whistles.
 * Throws MyError on errors.
 * @param pcszPrefix Descriptive short prefix string for console output.
 * @param pVirtualBox VirtualBox instance.
 * @param pcszOVF0 File to import.
 * @param llMachinesCreated out: UUIDs of machines that were created so that caller can clean up.
 */
void importOVF(const char *pcszPrefix,
               ComPtr<IVirtualBox> &pVirtualBox,
               const char *pcszOVF0,
               std::list<Guid> &llMachinesCreated)
{
    char szAbsOVF[RTPATH_MAX];
    RTPathAbs(pcszOVF0, szAbsOVF, sizeof(szAbsOVF));

    RTPrintf("%s: reading appliance \"%s\"...\n", pcszPrefix, szAbsOVF);
    ComPtr<IAppliance> pAppl;
    HRESULT rc = pVirtualBox->CreateAppliance(pAppl.asOutParam());
    if (FAILED(rc)) throw MyError(rc, "failed to create appliance\n");

    ComPtr<IProgress> pProgress;
    rc = pAppl->Read(Bstr(szAbsOVF), pProgress.asOutParam());
    if (FAILED(rc)) throw MyError(rc, "Appliance::Read() failed\n");
    rc = pProgress->WaitForCompletion(-1);
    if (FAILED(rc)) throw MyError(rc, "Progress::WaitForCompletion() failed\n");
    LONG rc2;
    pProgress->COMGETTER(ResultCode)(&rc2);
    if (FAILED(rc2)) throw MyError(rc2, "Appliance::Read() failed\n", pProgress);

    RTPrintf("%s: interpreting appliance \"%s\"...\n", pcszPrefix, szAbsOVF);
    rc = pAppl->Interpret();
    if (FAILED(rc)) throw MyError(rc, "Appliance::Interpret() failed\n");

    com::SafeIfaceArray<IVirtualSystemDescription> aDescriptions;
    rc = pAppl->COMGETTER(VirtualSystemDescriptions)(ComSafeArrayAsOutParam(aDescriptions));
    for (uint32_t u = 0;
         u < aDescriptions.size();
         ++u)
    {
        ComPtr<IVirtualSystemDescription> pVSys = aDescriptions[u];

        com::SafeArray<VirtualSystemDescriptionType_T> aTypes;
        com::SafeArray<BSTR> aRefs;
        com::SafeArray<BSTR> aOvfValues;
        com::SafeArray<BSTR> aVboxValues;
        com::SafeArray<BSTR> aExtraConfigValues;
        rc = pVSys->GetDescription(ComSafeArrayAsOutParam(aTypes),
                                   ComSafeArrayAsOutParam(aRefs),
                                   ComSafeArrayAsOutParam(aOvfValues),
                                   ComSafeArrayAsOutParam(aVboxValues),
                                   ComSafeArrayAsOutParam(aExtraConfigValues));
        if (FAILED(rc)) throw MyError(rc, "VirtualSystemDescription::GetDescription() failed\n");

        for (uint32_t u2 = 0;
             u2 < aTypes.size();
             ++u2)
        {
            const char *pcszType;

            VirtualSystemDescriptionType_T t = aTypes[u2];
            switch (t)
            {
                case VirtualSystemDescriptionType_OS:
                    pcszType = "ostype";
                break;

                case VirtualSystemDescriptionType_Name:
                    pcszType = "name";
                break;

                case VirtualSystemDescriptionType_Product:
                    pcszType = "product";
                break;

                case VirtualSystemDescriptionType_ProductUrl:
                    pcszType = "producturl";
                break;

                case VirtualSystemDescriptionType_Vendor:
                    pcszType = "vendor";
                break;

                case VirtualSystemDescriptionType_VendorUrl:
                    pcszType = "vendorurl";
                break;

                case VirtualSystemDescriptionType_Version:
                    pcszType = "version";
                break;

                case VirtualSystemDescriptionType_Description:
                    pcszType = "description";
                break;

                case VirtualSystemDescriptionType_License:
                    pcszType = "license";
                break;

                case VirtualSystemDescriptionType_CPU:
                    pcszType = "cpu";
                break;

                case VirtualSystemDescriptionType_Memory:
                    pcszType = "memory";
                break;

                case VirtualSystemDescriptionType_HardDiskControllerIDE:
                    pcszType = "ide";
                break;

                case VirtualSystemDescriptionType_HardDiskControllerSATA:
                    pcszType = "sata";
                break;

                case VirtualSystemDescriptionType_HardDiskControllerSAS:
                    pcszType = "sas";
                break;

                case VirtualSystemDescriptionType_HardDiskControllerSCSI:
                    pcszType = "scsi";
                break;

                case VirtualSystemDescriptionType_HardDiskImage:
                    pcszType = "hd";
                break;

                case VirtualSystemDescriptionType_CDROM:
                    pcszType = "cdrom";
                break;

                case VirtualSystemDescriptionType_Floppy:
                    pcszType = "floppy";
                break;

                case VirtualSystemDescriptionType_NetworkAdapter:
                    pcszType = "net";
                break;

                case VirtualSystemDescriptionType_USBController:
                    pcszType = "usb";
                break;

                case VirtualSystemDescriptionType_SoundCard:
                    pcszType = "sound";
                break;

                default:
                    throw MyError(E_UNEXPECTED, "Invalid VirtualSystemDescriptionType\n");
                break;
            }

            RTPrintf("  vsys %2u item %2u: type %2d (%s), ovf: \"%ls\", vbox: \"%ls\", extra: \"%ls\"\n",
                     u, u2, t, pcszType,
                     aOvfValues[u2],
                     aVboxValues[u2],
                     aExtraConfigValues[u2]);
        }
    }

    RTPrintf("%s: importing %d machine(s)...\n", pcszPrefix, aDescriptions.size());
    rc = pAppl->ImportMachines(pProgress.asOutParam());
    if (FAILED(rc)) throw MyError(rc, "Appliance::ImportMachines() failed\n");
    rc = pProgress->WaitForCompletion(-1);
    if (FAILED(rc)) throw MyError(rc, "Progress::WaitForCompletion() failed\n");
    pProgress->COMGETTER(ResultCode)(&rc2);
    if (FAILED(rc2)) throw MyError(rc2, "Progress::GetResultCode() failed\n");

    com::SafeArray<BSTR> aMachineUUIDs;
    rc = pAppl->COMGETTER(Machines)(ComSafeArrayAsOutParam(aMachineUUIDs));
    if (FAILED(rc)) throw MyError(rc, "Appliance::GetMachines() failed\n");

    for (size_t u = 0;
         u < aMachineUUIDs.size();
         ++u)
    {
        RTPrintf("%s: created machine %u: %ls\n", pcszPrefix, u, aMachineUUIDs[u]);
        llMachinesCreated.push_back(Guid(Bstr(aMachineUUIDs[u])));
    }

    RTPrintf("%s: success!\n", pcszPrefix);
}

/**
 * Copies ovf-testcases/ovf-dummy.vmdk to the given target and appends that
 * target as a string to the given list so that the caller can delete it
 * again later.
 * @param llFiles2Delete List of strings to append the target file path to.
 * @param pcszDest Target for dummy VMDK.
 */
void copyDummyDiskImage(std::list<Utf8Str> &llFiles2Delete, const char *pcszDest)
{
    int vrc = RTFileCopy("ovf-testcases/ovf-dummy.vmdk", pcszDest);
    if (RT_FAILURE(vrc)) throw MyError(0, Utf8StrFmt("Cannot copy ovf-dummy.vmdk to %s: %Rra\n", pcszDest, vrc).c_str());
    llFiles2Delete.push_back(pcszDest);
}

/**
 *
 * @param argc
 * @param argv[]
 * @return
 */
int main(int argc, char *argv[])
{
    RTR3Init();

    HRESULT rc = S_OK;

    std::list<Utf8Str> llFiles2Delete;
    std::list<Guid> llMachinesCreated;

    ComPtr<IVirtualBox> pVirtualBox;

    try
    {
        RTPrintf("Initializing COM...\n");
        rc = com::Initialize();
        if (FAILED(rc)) throw MyError(rc, "failed to initialize COM!\n");

        ComPtr<ISession> pSession;

        RTPrintf("Creating VirtualBox object...\n");
        rc = pVirtualBox.createLocalObject(CLSID_VirtualBox);
        if (FAILED(rc)) throw MyError(rc, "failed to create the VirtualBox object!\n");

        rc = pSession.createInprocObject(CLSID_Session);
        if (FAILED(rc)) throw MyError(rc, "failed to create a session object!\n");

        // create the event queue
        // (here it is necessary only to process remaining XPCOM/IPC events after the session is closed)
        EventQueue eventQ;

        // testcase 1: import ovf-joomla-0.9/joomla-1.1.4-ovf.ovf
        copyDummyDiskImage(llFiles2Delete, "ovf-testcases/ovf-joomla-0.9/joomla-1.1.4-ovf-0.vmdk");
        copyDummyDiskImage(llFiles2Delete, "ovf-testcases/ovf-joomla-0.9/joomla-1.1.4-ovf-1.vmdk");
        importOVF("joomla-0.9", pVirtualBox, "ovf-testcases/ovf-joomla-0.9/joomla-1.1.4-ovf.ovf", llMachinesCreated);

        // testcase 2: import ovf-winxp-vbox-sharedfolders/winxp.ovf
        copyDummyDiskImage(llFiles2Delete, "ovf-testcases/ovf-winxp-vbox-sharedfolders/Windows 5.1 XP 1 merged.vmdk");
        copyDummyDiskImage(llFiles2Delete, "ovf-testcases/ovf-winxp-vbox-sharedfolders/smallvdi.vmdk");
        importOVF("winxp-vbox-sharedfolders", pVirtualBox, "ovf-testcases/ovf-winxp-vbox-sharedfolders/winxp.ovf", llMachinesCreated);

        RTPrintf("Machine imports done, no errors. Cleaning up...\n");
    }
    catch (MyError &e)
    {
        rc = e.m_rc;
        RTPrintf("%s", e.m_str.c_str());
    }

    try
    {
        // clean up the machines created
        for (std::list<Guid>::const_iterator it = llMachinesCreated.begin();
             it != llMachinesCreated.end();
             ++it)
        {
//             const Guid &uuid = *it;
//             Bstr bstrUUID(uuid.toUtf16());
//             ComPtr<IMachine> pMachine;
//             rc = pVirtualBox->GetMachine(bstrUUID, pMachine.asOutParam());
//             if (FAILED(rc)) throw MyError(rc, "VirtualBox::FindMachine() failed\n");
//
//             SafeIfaceArray<IMediumAttachment> aMediumAttachments;
//             rc = pMachine->COMGETTER(MediumAttachments)(ComSafeArrayAsOutParam(aMediumAttachments));
//             if (FAILED(rc)) throw MyError(rc, "Machine::MediumAttachments() failed\n");
//
//             for (size_t u2 = 0;
//                  u2 < aMediumAttachments.size();
//                  ++u2)
//             {
//                 ComPtr<IMediumAttachment> pMediumAttachment(aMediumAttachments[u2]);
//                 ComPtr<IMedium> pMedium;
//                 rc = pMediumAttachment->COMGETTER(Medium)(pMedium.asOutParam());
//                 if (FAILED(rc)) throw MyError(rc, "MediumAttachment::GetMedium() failed\n");
//
//                 if (!pMedium.isNull())
//                 {
//                     Bstr bstrLocation;
//                     rc = pMedium->COMGETTER(Location)(bstrLocation.asOutParam());
//                     if (FAILED(rc)) throw MyError(rc, "Medium::GetLocation() failed\n");
//
//                     llFiles2Delete.push_back(bstrLocation);
//                 }
//             }

//             RTPrintf("  Deleting machine %ls...\n", bstrUUID.raw());
//             pVirtualBox->UnregisterMachine(bstrUUID, pMachine.asOutParam());
//             if (FAILED(rc)) throw MyError(rc, "VirtualBox::UnregisterMachine() failed\n");
//
//             if (!pMachine.isNull())
//             {
//                 rc = pMachine->DeleteSettings();
//                 if (FAILED(rc)) throw MyError(rc, "Machine::DeleteSettings() failed\n");
//             }
        }
    }
    catch (MyError &e)
    {
        rc = e.m_rc;
        RTPrintf("%s", e.m_str.c_str());
    }

    // clean up the VMDK copies that we made in copyDummyDiskImage()
    for (std::list<Utf8Str>::const_iterator it = llFiles2Delete.begin();
         it != llFiles2Delete.end();
         ++it)
    {
        const Utf8Str &strFile = *it;
        RTFileDelete(strFile.c_str());
    }

    pVirtualBox.setNull();

    RTPrintf("Shutting down COM...\n");
    com::Shutdown();
    RTPrintf ("tstOVF all done!\n");

    return rc;
}

