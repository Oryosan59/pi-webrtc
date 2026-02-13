import { useEffect, useRef, useState } from 'react';

// WebRTC Configuration (STUN server)
const rtcConfig: RTCConfiguration = {
    iceServers: [
        { urls: 'stun:stun.l.google.com:19302' }
    ]
};

export default function WebrtcViewer() {
    const videoRef = useRef<HTMLVideoElement>(null);
    const [status, setStatus] = useState<string>('Disconnected');
    const [wsConnected, setWsConnected] = useState(false);

    // Refs to hold instances without triggering re-renders
    const pcRef = useRef<RTCPeerConnection | null>(null);
    const wsRef = useRef<WebSocket | null>(null);

    const sendWsMessage = (msg: { type: string, data?: string }) => {
        if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
            wsRef.current.send(JSON.stringify(msg));
        }
    };

    const setupPeerConnection = () => {
        if (pcRef.current) return;

        console.log('Creating RTCPeerConnection');
        const pc = new RTCPeerConnection(rtcConfig);
        pcRef.current = pc;

        // Handle incoming video stream
        pc.ontrack = (event) => {
            console.log('Track received:', event.streams[0]);
            setStatus('Stream Received');

            // Low Latency: Set playoutDelayHint to 0
            if (event.receiver && (event.receiver as any).playoutDelayHint !== undefined) {
                (event.receiver as any).playoutDelayHint = 0;
            }

            if (videoRef.current) {
                videoRef.current.srcObject = event.streams[0];
            }
        };

        // Handle ICE Candidates
        pc.onicecandidate = (event) => {
            if (event.candidate) {
                // Send candidate to peer via WebSocket
                const candidateJson = JSON.stringify(event.candidate.toJSON());
                sendWsMessage({
                    type: 'ice',
                    data: candidateJson
                });
            }
        };

        // Connection state changes
        pc.onconnectionstatechange = () => {
            console.log('PC State:', pc.connectionState);
            setStatus('WebRTC State: ' + pc.connectionState);
        };
    };

    useEffect(() => {
        // 1. WebSocket Connection
        const ws = new WebSocket('ws://localhost:9001');
        wsRef.current = ws;

        ws.onopen = () => {
            console.log('WS Connected');
            setWsConnected(true);
            setStatus('Signaling Server Connected');
        };

        ws.onclose = () => {
            console.log('WS Disconnected');
            setWsConnected(false);
            setStatus('Disconnected');
        };

        ws.onmessage = async (event) => {
            const msg = JSON.parse(event.data);
            console.log('WS Message:', msg.type);

            if (!pcRef.current) {
                // Initialize PeerConnection if not exists
                setupPeerConnection();
            }

            const pc = pcRef.current!;

            try {
                if (msg.type === 'offer') {
                    // Handle Offer
                    setStatus('Received Offer from Pi');
                    await pc.setRemoteDescription(new RTCSessionDescription({ type: 'offer', sdp: msg.data }));

                    const answer = await pc.createAnswer();
                    await pc.setLocalDescription(answer);

                    // Send Answer
                    sendWsMessage({
                        type: 'answer',
                        data: answer.sdp
                    });
                    setStatus('Sent Answer to Pi');

                } else if (msg.type === 'ice') {
                    // Handle ICE Candidate
                    if (msg.data) { // Ensure data exists (candidate json string)
                        const candidateData = JSON.parse(msg.data);
                        await pc.addIceCandidate(new RTCIceCandidate(candidateData));
                        console.log('Added ICE Cand');
                    }
                }
            } catch (err) {
                console.error('Signaling Error:', err);
                setStatus('Error: ' + String(err));
            }
        };

        return () => {
            ws.close();
            if (pcRef.current) {
                pcRef.current.close();
                pcRef.current = null;
            }
        };
    }, []);

    return (
        <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: '1rem', padding: '20px' }}>
            <h1>Pi WebRTC Viewer</h1>
            <div style={{
                width: '100vw',
                height: '100vh',
                backgroundColor: '#000',
                overflow: 'hidden',
                position: 'fixed',
                top: 0,
                left: 0,
                zIndex: 0
            }}>
                <video
                    ref={videoRef}
                    autoPlay
                    playsInline
                    muted // Muted needed for autoplay policy
                    style={{ width: '100%', height: '100%', objectFit: 'contain' }}
                />
            </div>

            {/* Overlay Status */}
            <div style={{
                position: 'fixed',
                top: '10px',
                left: '10px',
                zIndex: 1,
                backgroundColor: 'rgba(0,0,0,0.5)',
                color: wsConnected ? '#4ade80' : '#f87171',
                padding: '5px 10px',
                borderRadius: '4px',
                fontSize: '12px',
                pointerEvents: 'none'
            }}>
                Status: {status}
            </div>
        </div>
    );
}
