#ifndef NS_STOCK_QUOTES
#define NS_STOCK_QUOTES
#include "ns_sql.h"

struct ns_quote{
	ns_quote(){}
	ns_quote(const string & txt,const string &auth):text(txt),author(auth){}
	string text,
		   author;
};
class ns_stock_quotes{
public:
	ns_stock_quotes(){
		quotes.push_back(ns_quote("It was, of course, a lie what you read about my religious convictions, a lie which is being systematically repeated. I do not believe in a personal God and I have never denied this but have expressed it clearly. If something is in me which can be called religious then it is the unbounded admiration for the structure of the world so far as our science can reveal it.",
						 "Albert Einstein"));
		quotes.push_back(ns_quote("The most beautiful thing we can experience is the mysterious. It is the source of all true art and all science. He to whom this emotion is a stranger, who can no longer pause to wonder and stand rapt in awe, is as good as dead: his eyes are closed.",
						 "Albert Einstein"));
		quotes.push_back(ns_quote("Nature composes some of her loveliest poems for the microscope and the telescope.",
						"Theodore Roszak"));
		quotes.push_back(ns_quote("Research is what I'm doing when I don't know what I'm doing.",
						"Wernher Von Braun"));
		quotes.push_back(ns_quote("Scientists should always state the opinions upon which their facts are based.",
						"Anon"));
		quotes.push_back(ns_quote("Science is built up of facts, as a house is built of stones; but an accumulation of facts is no more a science than a heap of stones is a house.",
						"Henri Poincare"));
		quotes.push_back(ns_quote("I am compelled to fear that science will be used to promote the power of dominant groups rather than to make men happy.",
						"Betrand Russell"));
		quotes.push_back(ns_quote("Science is all those things which are confirmed to such a degree that it would be unreasonable to withhold one's provisional consent.",
						"Stephen Jay Gould"));
		quotes.push_back(ns_quote("Research is the process of going up alleys to see if they are blind.",
						"Marston Bates"));
		quotes.push_back(ns_quote("...man will occasionally stumble over the truth, but usually manages to pick himself up, walk over or around it, and carry on. ",
						"Winston Churchill"));
		quotes.push_back(ns_quote("Nothing tends so much to the advancement of knowledge as the application of a new instrument. The native intellectual powers of men in different times are not so much the causes of the different success of their labours, as the peculiar nature of the means and artificial resources in their possession.",
						"Sir Humphrey Davy"));
		quotes.push_back(ns_quote("The technologies which have had the most profound effects on human life are usually simple. A good example of a simple technology with profound historical consequences is hay. Nobody knows who invented hay, the idea of cutting grass in the autumn and storing it in large enough quantities to keep horses and cows alive through the winter. All we know is that the technology of hay was unknown to the Roman Empire but was known to every village of medieval Europe...",
						"Freeman Dyson"));
		quotes.push_back(ns_quote("If it could be demonstrated that any complex organ existed which could not possibly have been formed by numerous, successive, slight modifications, my theory would absolutely break down.",
						"Charles Darwin"));
		quotes.push_back(ns_quote("Chance favors only the prepared mind.",
						"Louis Pasteur"));
		quotes.push_back(ns_quote("Biology is the only science in which multiplication means the same thing as division.",
						"Anon"));
		quotes.push_back(ns_quote("A rolling stone gathers no moss.",
						"Anon"));
		quotes.push_back(ns_quote("I must confess, I was born at a very early age. ",
						"Groucho Marx"));
		quotes.push_back(ns_quote("I'm not particularly fond of dying.  In fact, it's the last thing I'd want to do.",
						"Lord Palmerston"));
		quotes.push_back(ns_quote("Who are you going to believe, me or your own eyes?",
						"Groucho Marx"));
		quotes.push_back(ns_quote("Why should I care about posterity? What's posterity ever done for me?",
						"Groucho Marx"));
		quotes.push_back(ns_quote("A child of five would understand this. Send someone to fetch a child of five.",
						"Groucho Marx"));
		quotes.push_back(ns_quote("One of the saddest lessons of history is this: If we've been bamboozled long enough, we tend to reject any evidence of the bamboozle. We're no longer interested in finding out the truth. The bamboozle has captured us. It is simply too painful to acknowledge -- even to ourselves -- that we've been so credulous. (So the old bamboozles tend to persist as the new bamboozles rise.)",
						"Carl Sagan"));
		quotes.push_back(ns_quote("At the heart of science is an essential tension between two seemingly contradictory attitudes -- an openness to new ideas, no matter how bizarre or counterintuitive they may be, and the most ruthless skeptical scrutiny of all ideas, old and new. This is how deep truths are winnowed from deep nonsense.",
						"Carl Sagan"));
		quotes.push_back(ns_quote("I know that most men -- not only those considered clever, but even those who are very clever and capable of understanding most difficult scientific, mathematical, or philosophic, problems -- can seldom discern even the simplest and most obvious truth if it be such as obliges them to admit the falsity of conclusions they have formed, perhaps with much difficulty -- conclusions of which they are proud, which they have taught to others, and on which they have built their lives.",
						"Tolstoy"));
		quotes.push_back(ns_quote("There is something fascinating about science. One gets such wholesale returns of conjecture out of such a trifling investment of fact.",
						"Mark Twain"));
		quotes.push_back(ns_quote("The knack of flying is learning how to throw yourself at the ground and miss.",
					"Douglas Adams"));
		quotes.push_back(ns_quote("Ignorance more frequently begets confidence than does knowledge.",
					"Charles Darwin"));
		quotes.push_back(ns_quote("False facts are highly injurious to the progress of science, for they often endure long; but false views, if supported by some evidence, do little harm, for everyone takes a salutary pleasure in proving their falseness; and when this is done, one path towards error is closed and the road to truth is often at the same time opened.",
					"Charles Darwin"));
		quotes.push_back(ns_quote("Nothing is less productive than to make more efficient what should not be done at all.",
					"Peter Drucker"));
		quotes.push_back(ns_quote("In questions of science the authority of a thousand is not worth the humble reasoning of a single individual.",
					"Galileo Galilei"));
		quotes.push_back(ns_quote("It's only when you look at an ant through a magnifying glass on a sunny day you realise how often they burst into flames.",
					"Harry Hill"));
		quotes.push_back(ns_quote("The difference between truth and fiction: Fiction has to make sense.",
					"Mark Twain"));




	}
	unsigned long count(){
		return (unsigned long)quotes.size();
	}
	void submit_quotes(ns_sql & sql){
		//prevent the race condition of two nodes simultaneously submitting quotes

		ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("daily_quotes", &sql, true, __FILE__, __LINE__));

		try{
			unsigned long val = sql.get_integer_value("SELECT count(quote) FROM daily_quotes WHERE stock = 1");
			if (val != quotes.size()){
				sql.send_query("DELETE FROM daily_quotes WHERE stock = 1");
				for (unsigned long i = 0; i < (unsigned long)quotes.size(); i++){
					sql << "INSERT INTO daily_quotes SET quote='" << sql.escape_string(quotes[i].text) << "', author='" << sql.escape_string(quotes[i].author) << "', stock=1";
					sql.send_query();
				}
			}
			lock.release(__FILE__, __LINE__);
		}
		catch(...){
			sql.clear_query();
			lock.release(__FILE__, __LINE__);
			throw;
		}

	}
private:
	vector<ns_quote> quotes;
};

#endif
