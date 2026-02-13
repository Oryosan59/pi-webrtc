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
            <div style={{ marginBottom: '10px', color: wsConnected ? 'green' : 'red' }}>
                Status: {status}
            </div>

            <div style={{
                width: '100%',
                maxWidth: '800px',
                aspectRatio: '16/9',
                backgroundColor: '#000',
                borderRadius: '8px',
                overflow: 'hidden'
            }}>
                <video
                    ref={videoRef}
                    autoPlay
                    playsInline
                    muted // Muted needed for autoplay policy
                    style={{ width: '100%', height: '100%', objectFit: 'contain' }}
                />
            </div>
        </div>
    );
}
