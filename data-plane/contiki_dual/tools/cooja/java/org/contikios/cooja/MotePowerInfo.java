package org.contikios.cooja;

import org.contikios.cooja.interfaces.Radio;

import java.util.ArrayList;

/**
 * Created by wjuni on 2017. 1. 3..
 */
public class MotePowerInfo {

    private ArrayList<MotePowerInfoEntry> lst;

    public MotePowerInfo(){
       lst = new ArrayList<>();
    }

    public MotePowerInfoEntry getMotePowerEntry(int moteId){
        for (MotePowerInfoEntry me : lst){
            if (me.MoteID == moteId)
                return me;
        }
        return null;
    }

    public void updateMotePowerEntry(int moteId, long radioOn, long radioTx, long radioRx, long radioOnLong, long radioTxLong, long radioRxLong, long duration) {
        MotePowerInfoEntry e = getMotePowerEntry(moteId);
        if (e == null){
            e = new MotePowerInfoEntry(moteId);
            lst.add(e);
        }
        e.radioOn = radioOn;
        e.radioTx = radioTx;
        e.radioRx = radioRx;
        e.radioOnLong = radioOnLong;
        e.radioTxLong = radioTxLong;
        e.radioRxLong = radioRxLong;
        e.duration = duration;
    }
}
