package org.contikios.cooja;

/**
 * Created by wjuni on 2017. 1. 3..
 */
public class MotePowerInfoEntry {
    public int MoteID = 0;
    public long radioOn = 0;
    public long radioTx = 0;
    public long radioRx = 0;
    public long radioOnLong = 0;
    public long radioTxLong = 0;
    public long radioRxLong = 0;
    public long duration = 0;
    public MotePowerInfoEntry(int moteid){
        this.MoteID = moteid;
    }
}
