using System;
using System.Net;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using Windows.Data.Json;

namespace PeerConnectionClient.Signalling
{
	/// <summary>
	/// An OAuth24D authentication client
	/// </summary>
	public class OAuth24DClient : IDisposable
	{
		/// <summary>
		/// Represents the code completion data that is retrieved from
		/// a codeUri of a compatible oauth24d provider
		/// </summary>
		public struct CodeCompletionData
		{
			/// <summary>
			/// The underlying http status of the response
			/// </summary>
			public int http_status;

			/// <summary>
			/// The device_code that uniquely represents this device
			/// </summary>
			public string device_code;

			/// <summary>
			/// The user code that uniquely represents the request for user authentication
			/// </summary>
			public string user_code;

			/// <summary>
			/// The interval (in seconds) at which the oauth24d provider expects us to poll
			/// </summary>
			public double interval;

			/// <summary>
			/// The verification url of the oauth24d provider that a user should visit to provide user authentication
			/// </summary>
			public string verification_url;
		}

		/// <summary>
		/// Represents the auth completion data that is retrieved from
		/// a pollUri of a compatible oauth24d provider
		/// </summary>
		public struct AuthCompletionData
		{
			/// <summary>
			/// The underlying http status of the response
			/// </summary>
			public int http_status;

			/// <summary>
			/// The access code that grants our application permissions on behalf of a user
			/// </summary>
			public string access_code;
		}

		/// <summary>
		/// Event delegate for when an oauth24d code response is received
		/// </summary>
		/// <param name="data">the contents of the response</param>
		public delegate void OnCodeComplete(CodeCompletionData data);

		/// <summary>
		/// Fired when an oauth24d code response is received
		/// </summary>
		/// <remarks>
		/// Use <see cref="CodeCompletionData.http_status"/> to determine success or failure
		/// Triggered after a call to <see cref="Authenticate"/>
		/// </remarks>
		public event OnCodeComplete CodeComplete;

		/// <summary>
		/// Event delegate for when an oauth24d flow is complete
		/// </summary>
		/// <param name="data">the contents of the response</param>
		public delegate void OnAuthenticationComplete(AuthCompletionData data);

		/// <summary>
		/// Fired when an oauth24d flow is complete
		/// </summary>
		/// <remarks>
		/// Use <see cref="AuthCompletionData.http_status"/> to determine success or failure
		/// Triggered after a user interacts with the oauth24d provider after a <see cref="CodeComplete"/> event
		/// </remarks>
		public event OnAuthenticationComplete AuthenticationComplete;

		/// <summary>
		/// Internal representation of the oauth24d provider code distribution uri
		/// </summary>
		private string codeUri;

		/// <summary>
		/// Internal representation of the oauth24d provider access token polling uri
		/// </summary>
		private string pollUri;

		/// <summary>
		/// Internally indicates if the object has been disposed
		/// </summary>
		/// <remarks>
		/// This is part of the <see cref="IDisposable"/> pattern
		/// </remarks>
		private bool disposedValue;

		/// <summary>
		/// Internal representation of the http client used to communicate with the oauth24d provider
		/// </summary>
		private HttpClient client;

		/// <summary>
		/// Internal timer used to poll the oauth24d provider (at the pollUri) for an access token
		/// </summary>
		private Timer pollTimer;

		/// <summary>
		/// Default ctor
		/// </summary>
		/// <param name="codeUri">the oauth24d code distribution uri</param>
		/// <param name="pollUri">the oauth24d access token polling uri</param>
		public OAuth24DClient(string codeUri, string pollUri)
		{
			if (!Uri.IsWellFormedUriString(codeUri, UriKind.Absolute))
			{
				throw new ArgumentException(nameof(codeUri));
			}

			if (!Uri.IsWellFormedUriString(pollUri, UriKind.Absolute))
			{
				throw new ArgumentException(nameof(pollUri));
			}

			this.codeUri = codeUri;
			this.pollUri = pollUri;
			this.disposedValue = false;
			this.client = new HttpClient();
		}

		/// <summary>
		/// Triggers the authentication flow
		/// </summary>
		/// <remarks>
		/// This returns a boolean to remain compatible with our native
		/// implementation - however it always returns true in managed code
		/// </remarks>
		/// <returns><c>true</c></returns>
		public bool Authenticate()
		{
			this.client.GetAsync(this.codeUri).ContinueWith(async (Task<HttpResponseMessage> prev) =>
			{
				var message = prev.Result;

				var eventData = new CodeCompletionData()
				{
					http_status = (int)message.StatusCode
				};

				if (message.StatusCode == HttpStatusCode.OK)
				{
					var body = await message.Content.ReadAsStringAsync();
					var obj = JsonObject.Parse(body);
					
					eventData.device_code = obj["device_code"].GetString();
					eventData.user_code = obj["user_code"].GetString();
					eventData.interval = obj["interval"].GetNumber();
					eventData.verification_url = obj["verification_url"].GetString();

					// schedule our polling logic
					this.pollTimer = new Timer(PollTimer,
						eventData.device_code,
						TimeSpan.FromMilliseconds(0),
						TimeSpan.FromSeconds(eventData.interval));
				}

				this.CodeComplete?.Invoke(eventData);
			});

			// this maintains api compatibility with our native version
			// but is technically useless here (since it's always true)
			return true;
		}

		/// <summary>
		/// Internal method called by our pollTimer to poll the oauth24d access token polling uri
		/// </summary>
		/// <remarks>
		/// The state parameter is required by <see cref="TimerCallback"/>, but is always null here
		/// </remarks>
		/// <param name="state">the device_code to poll with, given as a string</param>
		private void PollTimer(object state)
		{
			var device_code = state as string;
			var queryToAppend = "device_code=" + device_code;
			var baseUri = new UriBuilder(this.pollUri);

			// <sigh>..see https://msdn.microsoft.com/en-us/library/system.uribuilder.query(v=vs.110).aspx
			if (baseUri.Query != null && baseUri.Query.Length > 1)
				baseUri.Query = baseUri.Query.Substring(1) + "&" + queryToAppend;
			else
				baseUri.Query = queryToAppend;
			
			this.client.GetAsync(baseUri.Uri).ContinueWith(async (Task<HttpResponseMessage> prev) =>
			{
				var message = prev.Result;

				var eventData = new AuthCompletionData()
				{
					http_status = (int)message.StatusCode
				};

				if (message.StatusCode == HttpStatusCode.OK)
				{
					var body = await message.Content.ReadAsStringAsync();
					var obj = JsonObject.Parse(body);

					eventData.access_code = obj["access_code"].GetString();
					
					// we don't need this anymore if we have success
					this.pollTimer.Dispose();
				}

				this.AuthenticationComplete?.Invoke(eventData);
			});
		}
		
		/// <summary>
		/// Implements the <see cref="IDisposable"/> pattern
		/// </summary>
		/// <param name="disposing">indicates if we're currently disposing the object</param>
		protected virtual void Dispose(bool disposing)
		{
			if (!disposedValue)
			{
				if (disposing)
				{
					this.client?.Dispose();
					this.pollTimer?.Dispose();
				}

				disposedValue = true;
			}
		}
		
		/// <summary>
		/// Dispose of the object
		/// </summary>
		public void Dispose()
		{
			Dispose(true);
		}
	}
}
