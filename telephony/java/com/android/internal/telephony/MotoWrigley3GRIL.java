/*
 * Copyright (C) 2012 The CyanogenMod Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.internal.telephony;

import android.content.Context;
import android.os.AsyncResult;
import android.os.Looper;
import android.os.Message;
import android.os.Parcel;
import android.telephony.PhoneNumberUtils;
import android.util.Log;

import com.android.internal.telephony.DataCallState;
import static com.android.internal.telephony.RILConstants.RIL_REQUEST_SETUP_DATA_CALL;
import com.android.internal.telephony.gsm.SuppServiceNotification;

import java.util.ArrayList;
import java.util.Collections;

/**
 * RIL class for Motorola Wrigley 3G RILs which need
 * supplementary service notification post-processing
 *
 * {@hide}
 */
public class MotoWrigley3GRIL extends RIL {
    private static final String TAG = "MotoWrigley3GRIL";

    private int mDataConnectionCount = -1;
    private RILRequest mSetupDataCallRequest;

    public MotoWrigley3GRIL(Context context, int networkMode, int cdmaSubscription) {
        super(context, networkMode, cdmaSubscription);
        mSender = new RILSender(mSenderThread.getLooper());
    }

    @Override
    protected RILRequest
    findAndRemoveRequestFromList(int serial) {
        RILRequest rr = super.findAndRemoveRequestFromList(serial);
        if (rr == mSetupDataCallRequest) {
            /*
             * either response arrived, or there was an error that was already handled -
             * either way, the upper layers were notified already
             */
            Log.d(TAG, "Got SETUP_DATA_CALL response");
            mSetupDataCallRequest = null;
        }
        return rr;
    }

    @Override
    protected Object
    responseSuppServiceNotification(Parcel p) {
        SuppServiceNotification notification =
                (SuppServiceNotification) super.responseSuppServiceNotification(p);

        /**
         * Moto's RIL seems to confuse code2 0 ('forwarded call') and
         * 10 ('additional incoming call forwarded') and sends 10 when an
         * incoming call is forwarded and _no_ call is currently active.
         * It never sends 10 where it would be appropriate, so it's safe
         * to just convert every occurence of 10 to 0.
         */
        if (notification.notificationType == 1) {
            if (notification.code == SuppServiceNotification.MT_CODE_ADDITIONAL_CALL_FORWARDED) {
                notification.code = SuppServiceNotification.MT_CODE_FORWARDED_CALL;
            }
        }

        return notification;
    }

    @Override
    protected Object
    responseCallList(Parcel p) {
        int num;
        int voiceSettings;
        ArrayList<DriverCall> response;
        DriverCall dc;

        num = p.readInt();
        response = new ArrayList<DriverCall>(num);

        for (int i = 0 ; i < num ; i++) {
            dc = new DriverCall();

            dc.state = DriverCall.stateFromCLCC(p.readInt());
            dc.index = p.readInt();
            dc.TOA = p.readInt();
            dc.isMpty = (0 != p.readInt());
            dc.isMT = (0 != p.readInt());
            dc.als = p.readInt();
            voiceSettings = p.readInt();
            dc.isVoice = (0 == voiceSettings) ? false : true;
            dc.isVoicePrivacy = (0 != p.readInt());
            dc.number = p.readString();
            int np = p.readInt();
            /** numberPresentation needs to be overriden for outgoing calls
                in case of Moto Wrigley3G RIL under ICS, to prevent outgoing calls
                to be identified as "Unknown" on InCallScreen and in the call logs
                when CallerID option is set to "Network default" or "Hide number". **/
            if (!dc.isMT) np = 0;
            dc.numberPresentation = DriverCall.presentationFromCLIP(np);
            dc.name = p.readString();
            dc.namePresentation = p.readInt();
            int uusInfoPresent = p.readInt();
            if (uusInfoPresent == 1) {
                dc.uusInfo = new UUSInfo();
                dc.uusInfo.setType(p.readInt());
                dc.uusInfo.setDcs(p.readInt());
                byte[] userData = p.createByteArray();
                dc.uusInfo.setUserData(userData);
                riljLogv(String.format("Incoming UUS : type=%d, dcs=%d, length=%d",
                                dc.uusInfo.getType(), dc.uusInfo.getDcs(),
                                dc.uusInfo.getUserData().length));
                riljLogv("Incoming UUS : data (string)="
                        + new String(dc.uusInfo.getUserData()));
                riljLogv("Incoming UUS : data (hex): "
                        + IccUtils.bytesToHexString(dc.uusInfo.getUserData()));
            } else {
                riljLogv("Incoming UUS : NOT present!");
            }

            // Make sure there's a leading + on addresses with a TOA of 145
            dc.number = PhoneNumberUtils.stringFromStringAndTOA(dc.number, dc.TOA);

            response.add(dc);

            if (dc.isVoicePrivacy) {
                mVoicePrivacyOnRegistrants.notifyRegistrants();
                riljLog("InCall VoicePrivacy is enabled");
            } else {
                mVoicePrivacyOffRegistrants.notifyRegistrants();
                riljLog("InCall VoicePrivacy is disabled");
            }
        }

        Collections.sort(response);

        return response;
    }

    @Override
    protected Object
    responseDataCallList(Parcel p) {
        ArrayList<DataCallState> response =
                (ArrayList<DataCallState>) super.responseDataCallList(p);
        mDataConnectionCount = response.size();
        Log.d(TAG, "Got data call list message, now has " + mDataConnectionCount + " connections");

        return response;
    }

    @Override
    public void
    setupDataCall(String radioTechnology, String profile, String apn,
            String user, String password, String authType, String protocol,
            Message result) {
        RILRequest rr
                = RILRequest.obtain(RIL_REQUEST_SETUP_DATA_CALL, result);

        rr.mp.writeInt(7);

        rr.mp.writeString(radioTechnology);
        rr.mp.writeString(profile);
        rr.mp.writeString(apn);
        rr.mp.writeString(user);
        rr.mp.writeString(password);
        rr.mp.writeString(authType);
        rr.mp.writeString(protocol);

        if (RILJ_LOGD) riljLog(rr.serialString() + "> "
                + requestToString(rr.mRequest) + " " + radioTechnology + " "
                + profile + " " + apn + " " + user + " "
                + password + " " + authType + " " + protocol);

        mSetupDataCallRequest = rr;

        send(rr);
    }

    @Override
    public void
    deactivateDataCall(int cid, int reason, Message result) {
        if (mDataConnectionCount == 0) {
            Log.w(TAG, "Received deactivateDataCall RIL call without an active data call, dropping");
            AsyncResult.forMessage(result, null, null);
            result.sendToTarget();
        } else {
            super.deactivateDataCall(cid, reason, result);
        }
    }

    class RILSender extends RIL.RILSender {
        public RILSender(Looper looper) {
            super(looper);
        }

        @Override
        public void
        handleMessage(Message msg) {
            super.handleMessage(msg);

            if (msg.what == EVENT_WAKE_LOCK_TIMEOUT && mSetupDataCallRequest != null) {
                MotoWrigley3GRIL.super.findAndRemoveRequestFromList(mSetupDataCallRequest.mSerial);
                if (mSetupDataCallRequest.mResult != null) {
                    Log.e(TAG, "Got stale SETUP_DATA_CALL request, pretending radio not available");
                    CommandException ex = new CommandException(CommandException.Error.RADIO_NOT_AVAILABLE);
                    AsyncResult.forMessage(mSetupDataCallRequest.mResult, null, ex);
                    mSetupDataCallRequest.mResult.sendToTarget();
                }
                mSetupDataCallRequest.release();
                mSetupDataCallRequest = null;
            }
        }
    }
}
